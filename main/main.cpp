#include "ElabelController.hpp"
#include "m_esp_now.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "ui.h"
#include "lvgl_helpers.h"
#include "ssd1680.h"

#include "lvgl.h"
#include "network.h"
#include "http.h"
#include "ota.h"
#include "m_mqtt.h"
#include "global_message.h"
#include "esp_http_client.h"
#include "control_driver.h"

void print_memory_status() {
    // MALLOC_CAP_8BIT
    // 表示支持字节级访问的内存。
    // 通常用于通用内存分配。

    // MALLOC_CAP_DMA
    // 表示支持直接内存访问（DMA）的内存。
    // 这些内存区域适用于外设缓冲区（如 SPI、I2C）。

    // MALLOC_CAP_INTERNAL
    // 表示芯片内部内存（IRAM 或 DRAM）。
    // 具有高性能，适合实时任务或性能敏感的操作。

    // MALLOC_CAP_SPIRAM
    // 表示外部 SPI RAM（PSRAM），通常比内部内存更大，但访问速度较慢。
    // 适合存储大量数据或低优先级任务的缓冲区。
    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI("MEM", "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
}


/* 创建一个 semaphore 来处理对 lvgl 的并发调用。
* 如果你想从其他线程/任务中调用 *any* lvgl 函数
* 则应锁定同一个信号！*/
static void guiTask(void *pvParameter) {
    (void) pvParameter;
    Inituilock();
    //--------------------------lvgl初始化--------------------------//
    lv_init();
    //--------------------------lvgl初始化--------------------------//


    //---------------lvgl硬件初始化定义在lvgl_esp32_drivers，这个函数里面会根据宏定义调用自己定义的ssd1680的函数---------------//
    lvgl_driver_init();
    //---------------lvgl硬件初始化定义在lvgl_esp32_drivers，这个函数里面会根据宏定义调用自己定义的ssd1680的函数---------------//


    //--------------------------初始化显示缓冲区--------------------------//
    //分配可用于直接内存访问（DMA）的内存，通常用于高速外设（如 SPI 或显示设备）
    //lv_color_t 用于存储颜色，这里根据宏定义为lv_color1_t 单色颜色。也有 R、G、B 字段用于兼容，但它们总是相同的值（1 个字节）
    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),MALLOC_CAP_DMA); 
    assert(buf1 != NULL);
    //一般单色屏幕只用单buffer
    #ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
        lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
        assert(buf2 != NULL);
    #else
        static lv_color_t *buf2 = NULL;
    #endif

    static lv_disp_draw_buf_t disp_buf;

    //DISP_BUF_SIZE定义为屏幕尺寸/8,表示有多少字节
    uint32_t size_in_px = DISP_BUF_SIZE;
    //实际大小（单位：像素，而不是字节
    size_in_px *= 8;
    //disp_buf: 缓冲区对象。
    // buf1: 主缓冲区，存储显示的数据。
    // buf2: 可选的第二缓冲区（双缓冲），此处为 NULL，表示使用单缓冲。
    // size_in_px: 缓冲区的大小，以像素为单位。
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);
    //--------------------------初始化显示缓冲区--------------------------//


    //--------------------------初始化显示驱动器--------------------------//
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = LV_VER_RES_MAX;
    disp_drv.ver_res = LV_HOR_RES_MAX;

    // 单色显示器的额外回调:
    //  - rounder_cb
    //  - set_px_cb 
    //set_px_cb: 设置单个像素的回调函数，适用于单色显示器。
    //rounder_cb: 调整绘制区域的边界，使其适应显示器特性（如字节对齐）。
    disp_drv.set_px_cb = disp_driver_set_px;
    disp_drv.rounder_cb = disp_driver_rounder;

    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    //--------------------------初始化显示驱动器--------------------------//


    //--------------创建并启动一个定时器，用于定期调用 lv_tick_task 函数(每隔 1 毫秒触发一次),来告诉lvgl过了1ms--------------//
    esp_timer_handle_t periodic_timer;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = [](void *arg){
        (void)arg;
        lv_tick_inc(1);},
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic_gui",
        .skip_unhandled_events = false};
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));
    //--------------创建并启动一个定时器，用于定期调用 lv_tick_task 函数(每隔 1 毫秒触发一次),来告诉lvgl过了1ms--------------//


    //--------------开始lvgl线程--------------//
    ESP_LOGI("lvgl", "start gui task.\n");
    ui_init();
    lv_refr_now(NULL);
    while (1) {
        /* lvgl刷新率10ms*/
        vTaskDelay(pdMS_TO_TICKS(10));
        /* Try to take the semaphore, call lvgl related function on success */
        lock_lvgl();
        lv_task_handler();
        release_lvgl();
    }
    //当函数出来之后，就可以释放内存了
    free(buf1);
    //因为是单色的屏幕，没有buf2所以不用释放buf2
    #ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
        free(buf2);
    #endif
    vTaskDelete(NULL);
}


extern "C" void app_main(void)
{
    //初始化wifi
    m_wifi_init();
    //连接wifi
    m_wifi_connect();
    //创建httpclient更新线程
    http_client_init();
    //按键初始化
    control_gpio_init();
    //创造gui线程，这里需要绑定到一个核上防止内存被破坏，这里优先绑定到核1上，如果蓝牙和wifi不用的情况下可以绑定到核0上
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
    //这里在调用完初始化后会紧跟一个刷白的回调函数
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    //等待2slvgl初始化完成
    ElabelController::Instance()->Init();//Elabel控制器初始化
    elabelUpdateTick = 0;
    set_led_state(device_init);
    while(1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        elabelUpdateTick += 10;
        //20ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
        if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();
    }
}


