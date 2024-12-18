/**
 * @file disp_spi.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define TAG "disp_spi"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "disp_spi.h"
#include "disp_driver.h"

#include "../lvgl_helpers.h"
#include "../lvgl_spi_conf.h"

/******************************************************************************
关于 DMA spi_transaction_ext_t 结构体池的说明：

使用了一个 xQueue 来存放一个可重用的 SPI spi_transaction_ext_t 结构体池，这些结构体将用于所有 DMA SPI 事务。
虽然 xQueue 看起来可能有些过于复杂，但它是一个内置的 RTOS 功能，使用成本很低。xQueue 还具有 ISR 安全性（中断服务程序），如果需要在 ISR 回调中访问池，这一点非常重要。

当发送 DMA 请求时，会从池中取出一个事务结构，填充相关信息并传递给 ESP32 SPI 驱动程序。稍后（笑了，这个稍好是很久之后，不是处理完马上返回，要等到溢出50上限后，全部返回清理），在处理待处理的 SPI 事务结果时，该事务结构会被回收到池中，供以后重用。这符合 ESP32 SPI 驱动的 DMA SPI 事务生命周期要求。

在轮询或同步发送 SPI 请求时，并且根据 ESP32 SPI 驱动的要求，所有待处理的 DMA 事务会首先被处理，然后才会进行轮询的 SPI 请求。（就是说spi_device_polling_transmit和spi_device_transmit调用前要满足spi_device_queue_trans添加的数据要被处理完）

在发送异步 DMA SPI 请求时，如果池为空，会先处理一些少量的待处理事务，然后再发送新的 DMA SPI 事务。这个平衡需要确保既不会处理过多也不会处理过少，因为它控制着 DMA 事务的延迟。

因此，设计并不是要求所有待处理的事务都必须被处理并放回池中，所有事务最终都会被处理回到池中（代码定义为当有40个spi被使用的时候就清理一次）。池中只需保持足够的结构体来支持一些在飞行中的 SPI 请求，以提高 DMA SPI 数据速率并减少事务延迟。然而，如果显示驱动使用了一些轮询的 SPI 请求或直接调用了 disp_wait_for_pending_transactions()，那么池可能会更频繁地达到满池状态，从而加快 DMA 排队速度。
 * 
 *****************************************************************************/
//对于esp32，SPI 总线传输事务由五个阶段构成，详见下表****（任意阶段均可跳过）****，如果全双工读取和写入阶段同步进行，如果半双工读取和写入阶段独立进行（一次一个方向）
// 1.命令阶段 (Command)
// 在此阶段，主机向总线发送命令字段，长度为 0-16 位。

// 2.地址阶段 (Address)
// 在此阶段，主机向总线发送地址字段，长度为 0-32 位。

// 3.Dummy 阶段
// 此阶段可自行配置，用于适配时序要求。

// 4.写入阶段 (Write)
// 此阶段主机向设备传输数据，这些数据在紧随命令阶段（可选）和地址阶段（可选）之后。从电平的角度来看，数据与命令没有区别。

// 5.读取阶段 (Read)
// 此阶段主机读取设备数据。


//主管整个流程的结构体为spi_transaction_t。在事务结束之前，不应修改该描述符


/*********************
 *      DEFINES
 *********************/
#define SPI_TRANSACTION_POOL_SIZE 50	/* maximum number of DMA transactions simultaneously in-flight */

//在队列其他 DMA 事务之前保留的 DMA 事务。1/10 似乎是一个很好的平衡。太多（或全部）会增加延迟
#define SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE 10
#if SPI_TRANSACTION_POOL_SIZE >= SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE
#define SPI_TRANSACTION_POOL_RESERVE (SPI_TRANSACTION_POOL_SIZE / SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE)	
#else
#define SPI_TRANSACTION_POOL_RESERVE 1	/* defines minimum size */
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR spi_ready (spi_transaction_t *trans);

/**********************
 *  STATIC VARIABLES
 **********************/
static spi_host_device_t spi_host;
static spi_device_handle_t spi;
static QueueHandle_t TransactionPool = NULL;
static transaction_cb_t chained_post_cb;

//看完之后我大受震撼，你的注释还真没错，TransactionPool队列吊用没有，我觉得唯一的作用是可以保证所有声明的transaction_cb_t的内存都是DMA内存，而且只是用于DISP_SPI_SEND_QUEUED 模式。
//所有入列出列的TransactionPool的spi事件都没有意义，TransactionPool唯一的好处是额外的计数有多少spi事件被发送，并且给出来的内存都是dma内存，并且保证到50上限的时候调用get——result清理队列

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void disp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg)
{
    spi_host=host;
    chained_post_cb=devcfg->post_cb;
    devcfg->post_cb=spi_ready;
    esp_err_t ret=spi_bus_add_device(host, devcfg, &spi);
    assert(ret==ESP_OK);
}

void disp_spi_add_device(spi_host_device_t host)
{
    disp_spi_add_device_with_speed(host, SPI_TFT_CLOCK_SPEED_HZ);
}

//初始化一个挂载的spi设备，不同的cs表示不同的spi设备，即使他们的spi主机相同
//在没有数据输出延迟的情况下，与理想从机通信的典型最高频率： 80MHz（IOMUX 引脚）和 26MHz（GPIO 矩阵引脚）
void disp_spi_add_device_with_speed(spi_host_device_t host, int clock_speed_hz)
{
    ESP_LOGI(TAG, "Adding SPI device");
    ESP_LOGI(TAG, "Clock speed: %dHz, mode: %d, CS pin: %d",
        clock_speed_hz, SPI_TFT_SPI_MODE, DISP_SPI_CS);
    spi_device_interface_config_t devcfg={
        .clock_speed_hz = clock_speed_hz,        //设置 SPI 时钟的频率，单位为赫兹（Hz）。这决定了 SPI 通信的传输速率。
        .mode = SPI_TFT_SPI_MODE,                //SPI 模式，用于指定 SPI 数据传输时的时钟极性（CPOL）和时钟相位（CPHA）,主从机的模式要一致，现在是时钟极性为0，相位为0。表示第一个CLK上升沿开始采样，空闲电平为低电平。
        .spics_io_num=DISP_SPI_CS,               // 指定片选（CS）引脚，用于选择 SPI 设备。
        .input_delay_ns=DISP_SPI_INPUT_DELAY_NS, //数据无延迟, 从站最大数据有效时间。从 SCLK 发射边沿到 MISO 数据有效之间的输入延迟，包括从站到主站之间可能存在的时钟延迟。驱动程序使用该值在 MISO 在线路上就绪之前提供额外的延迟。除非您知道需要延迟，否则请保持为 0。为了在高频率（8MHz 以上）下获得更好的定时性能，建议使用正确的值。
        .queue_size=SPI_TRANSACTION_POOL_SIZE,   //
        .pre_cb=NULL,                            //预回调函数。在 SPI 事务开始前调用的函数，可以用来进行一些准备工作（例如设置某些硬件状态）。
        .post_cb=NULL,                           //后回调函数。在 SPI 事务完成后调用的函数，通常用来处理事务完成后的清理或通知。这里定义为null，其实是在后面被赋值。这里的null表示的是后调函数的后调函数为null即上面说的chained_post_cb，估计是在完成基本回调函数spi_ready后的客户定制需求。
        // 并非每个传输事务都需要写入和读取数据，因此读取和写入阶段也是可选项,默认都是有的，除非定义
        //.flags |= SPI_TRANS_VARIABLE_ADDR; // 关闭 address mode
        //.flags &= ~SPI_TRANS_COMMAND_MODE; // 关闭 command mode
#if defined(DISP_SPI_HALF_DUPLEX)
        .flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX,	// 半双工模式，并指示不使用dummy位 
#else
	#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)
		.flags = 0,
	#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875)
        .flags = SPI_DEVICE_NO_DUMMY,
	#endif
#endif
    };

    disp_spi_add_device_config(host, &devcfg);

	//创建事务池，并在其中填入 spi_transaction_ext_t 的 ptrs 以重复使用
	if(TransactionPool == NULL) {
		TransactionPool = xQueueCreate(SPI_TRANSACTION_POOL_SIZE, sizeof(spi_transaction_ext_t*));
		assert(TransactionPool != NULL);
		for (size_t i = 0; i < SPI_TRANSACTION_POOL_SIZE; i++)
		{
            //使用 MALLOC_CAP_DMA 标志分配适合与硬件 DMA 引擎（如 SPI 和 I2S）配合使用的内存,这部分内存可以使用DMA搬运
			spi_transaction_ext_t* pTransaction = (spi_transaction_ext_t*)heap_caps_malloc(sizeof(spi_transaction_ext_t), MALLOC_CAP_DMA);
			assert(pTransaction != NULL);
			memset(pTransaction, 0, sizeof(spi_transaction_ext_t));
			xQueueSend(TransactionPool, &pTransaction, portMAX_DELAY);
		}
	}
}

void disp_spi_change_device_speed(int clock_speed_hz)
{
    if (clock_speed_hz <= 0) {
        clock_speed_hz = SPI_TFT_CLOCK_SPEED_HZ;
    }
    ESP_LOGI(TAG, "Changing SPI device clock speed: %d", clock_speed_hz);
    disp_spi_remove_device();
    disp_spi_add_device_with_speed(spi_host, clock_speed_hz);
}

void disp_spi_remove_device()
{
    /* 等待之前所有的任务处理完 */
    disp_wait_for_pending_transactions();

    esp_err_t ret=spi_bus_remove_device(spi);
    assert(ret==ESP_OK);
}


void disp_spi_transaction(const uint8_t *data, size_t length,
    disp_spi_send_flag_t flags, uint8_t *out,
    uint64_t addr, uint8_t dummy_bits)
{
//..............................................包装传入的spi事件..............................................//
    if (0 == length) {
        return;
    }
    //这个变量就是spi事件的载体，包含四个组成，address长度，command长度，spi信息载体，dummy_bits长度
    spi_transaction_ext_t t = {0};

    /* 发送的长度是以bit为单位而不是字节 */
    t.base.length = length * 8;

    //如果小于四个字节的信息，直接使用tx_data发送，而不是使用tx_data来作为信息载体
    if (length <= 4 && data != NULL) {
        t.base.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.base.tx_data, data, length);
    } else {
        t.base.tx_buffer = data;
    }

    if (flags & DISP_SPI_RECEIVE) {
        assert(out != NULL && (flags & (DISP_SPI_SEND_POLLING | DISP_SPI_SEND_SYNCHRONOUS)));
        t.base.rx_buffer = out;

#if defined(DISP_SPI_HALF_DUPLEX)
		t.base.rxlength = t.base.length;
		t.base.length = 0;	/* no MOSI phase in half-duplex reads */
#else
		t.base.rxlength = 0; /* in full-duplex mode, zero means same as tx length */
#endif
    }

    if (flags & DISP_SPI_ADDRESS_8) {
        t.address_bits = 8;
    } else if (flags & DISP_SPI_ADDRESS_16) {
        t.address_bits = 16;
    } else if (flags & DISP_SPI_ADDRESS_24) {
        t.address_bits = 24;
    } else if (flags & DISP_SPI_ADDRESS_32) {
        t.address_bits = 32;
    }
    if (t.address_bits) {
        t.base.addr = addr;
        t.base.flags |= SPI_TRANS_VARIABLE_ADDR;
    }

#if defined(DISP_SPI_HALF_DUPLEX)
	if (flags & DISP_SPI_MODE_DIO) {
		t.base.flags |= SPI_TRANS_MODE_DIO;
	} else if (flags & DISP_SPI_MODE_QIO) {
		t.base.flags |= SPI_TRANS_MODE_QIO;
	}

	if (flags & DISP_SPI_MODE_DIOQIO_ADDR) {
		t.base.flags |= SPI_TRANS_MODE_DIOQIO_ADDR;
	}

	if ((flags & DISP_SPI_VARIABLE_DUMMY) && dummy_bits) {
		t.dummy_bits = dummy_bits;
		t.base.flags |= SPI_TRANS_VARIABLE_DUMMY;
	}
#endif
    /* Save flags for pre/post transaction processing */
    t.base.user = (void *) flags;
//..............................................包装传入的spi事件..............................................//

    //有三种方法发送spi
    //1.调用函数 spi_device_polling_transmit() 发送轮询传输事务。若有插入内容的需要，也可使用 spi_device_polling_start() 和 spi_device_polling_end() 发送传输事务。
    //2.调用函数 spi_device_queue_trans() 将传输事务添加到队列中，随后使用函数 spi_device_get_trans_result() 查询结果,需要注意的是一定要通过spi_device_get_trans_result()获取结果，要不然完成的spi事件不会从内置队列中消失
    if (flags & DISP_SPI_SEND_POLLING) {
		disp_wait_for_pending_transactions();	/* before polling, all previous pending transactions need to be serviced */
        spi_device_polling_transmit(spi, (spi_transaction_t *) &t);
    } else if (flags & DISP_SPI_SEND_SYNCHRONOUS) {
		disp_wait_for_pending_transactions();	/* before synchronous queueing, all previous pending transactions need to be serviced */
        spi_device_transmit(spi, (spi_transaction_t *) &t);
    } else {
		/* if necessary, ensure we can queue new transactions by servicing some previous transactions */
		if(uxQueueMessagesWaiting(TransactionPool) == 0) {
			spi_transaction_t *presult;
			while(uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_RESERVE) {
				if (spi_device_get_trans_result(spi, &presult, 1) == ESP_OK) {
					xQueueSend(TransactionPool, &presult, portMAX_DELAY);	/* back to the pool to be reused */
				}
			}
		}

		spi_transaction_ext_t *pTransaction = NULL;
		xQueueReceive(TransactionPool, &pTransaction, portMAX_DELAY);
        memcpy(pTransaction, &t, sizeof(t));
        if (spi_device_queue_trans(spi, (spi_transaction_t *) pTransaction, portMAX_DELAY) != ESP_OK) {
			xQueueSend(TransactionPool, &pTransaction, portMAX_DELAY);	/* send failed transaction back to the pool to be reused */
        }
    }
}


void disp_wait_for_pending_transactions(void)
{
    //spi内容结构体
    spi_transaction_t *presult;
    //直到队列中的信息数量小于 SPI_TRANSACTION_POOL_SIZE
	while(uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_SIZE) {	/* service until the transaction reuse pool is full again */
        if (spi_device_get_trans_result(spi, &presult, 1) == ESP_OK) {
            //handle: 这是与 SPI 设备关联的句柄，通常在调用 spi_bus_add_device 时获得。
            //trans: 输出参数，用于接收 SPI 事务的结构体指针。这个结构体包含了与 SPI 事务相关的信息，比如数据传输的内容和状态。
			xQueueSend(TransactionPool, &presult, portMAX_DELAY);
        }
    }
}

void disp_spi_acquire(void)
{
    esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    assert(ret == ESP_OK);
}

void disp_spi_release(void)
{
    spi_device_release_bus(spi);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void IRAM_ATTR spi_ready(spi_transaction_t *trans)
{
    disp_spi_send_flag_t flags = (disp_spi_send_flag_t) trans->user;
//判断刷新完成是否要通知lvgl
    if (flags & DISP_SPI_SIGNAL_FLUSH) {
        lv_disp_t * disp = NULL;
//根据 LVGL 的版本获取当前正在刷新的显示对象。不同版本的 LVGL 可能有不同的函数来获取这个对象。
#if (LVGL_VERSION_MAJOR >= 7)
        disp = _lv_refr_get_disp_refreshing();
#else /* Before v7 */
        disp = lv_refr_get_disp_refreshing();
#endif

//调用 lv_disp_flush_ready 函数，通知 LVGL 当前的显示刷新已完成。根据 LVGL 的版本，调用的方式略有不同。
#if LVGL_VERSION_MAJOR < 8
        lv_disp_flush_ready(&disp->driver);
#else
        lv_disp_flush_ready(disp->driver);
#endif
    }

    if (chained_post_cb) {
        chained_post_cb(trans);
    }
}

