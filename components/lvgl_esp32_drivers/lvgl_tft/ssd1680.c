#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1680.h"
#include "bitmap.h"
#include "../ui/ui.h"

#define TAG "SSD1680"

#define BIT_SET(a,b)                    ((a) |= (1U<<(b)))
#define BIT_CLEAR(a,b)                  ((a) &= ~(1U<<(b)))

// pixels的数目
#define SSD1680_PIXEL                   (LV_HOR_RES_MAX * LV_VER_RES_MAX)

#define EPD_PANEL_NUMOF_COLUMS		EPD_PANEL_WIDTH
#define EPD_PANEL_NUMOF_ROWS_PER_PAGE	8

/* Are pages the number of bytes to represent the panel width? in bytes */
#define EPD_PANEL_NUMOF_PAGES	        (EPD_PANEL_HEIGHT / EPD_PANEL_NUMOF_ROWS_PER_PAGE)

#define SSD1680_PANEL_FIRST_PAGE	        0
#define SSD1680_PANEL_LAST_PAGE	        (EPD_PANEL_NUMOF_PAGES - 1)
#define SSD1680_PANEL_FIRST_GATE	        0
#define SSD1680_PANEL_LAST_GATE	        (EPD_PANEL_NUMOF_COLUMS - 1)

#define SSD1680_PIXELS_PER_BYTE		    8

// static uint8_t ssd1680_border_init[] = {0x05}; //init
static uint8_t ssd1680_border_part[] = {0x80}; //partial update
//A2 - 1 Follow LUT; A1:A0 - 01 LUT1;



screen elabel_screen;
bitmap_state elabel_bitmap_state;
partial_area elabel_partial_area;
update_mode elabel_update_mode;
bool isBaseMapFresh = false;

void set_bit_map_state(bitmap_state _bitmap_state)
{
    elabel_bitmap_state = _bitmap_state;
}

void set_partial_area(partial_area _partial_area)
{
    elabel_partial_area = _partial_area;
}




static void ssd1680_update_display();
static inline void ssd1680_set_window( uint16_t sx, uint16_t ex, uint16_t ys, uint16_t ye);
static inline void ssd1680_set_cursor(uint16_t sx, uint16_t ys);
static inline void ssd1680_waitbusy(int wait_ms);
static inline void ssd1680_hw_reset(void);
static inline void ssd1680_command_mode(void);
static inline void ssd1680_data_mode(void);
static inline void ssd1680_write_cmd(uint8_t cmd, uint8_t *data, size_t len);
static inline void ssd1680_send_cmd(uint8_t cmd);
static inline void ssd1680_send_data(uint8_t *data, uint16_t length);


// 当 LVGL 将一帧的图形渲染到内存缓冲区后，flush_cb 被调用，目的是将缓冲区中的像素数据刷新到屏幕。
// struct _lv_disp_drv_t * disp_drv为显示驱动器结构体

// lv_area_t 是一个矩形区域，定义了刷新操作的最小边界。
// typedef struct {
//     lv_coord_t x1; // 左上角 x 坐标
//     lv_coord_t y1; // 左上角 y 坐标
//     lv_coord_t x2; // 右下角 x 坐标
//     lv_coord_t y2; // 右下角 y 坐标
// } lv_area_t;

//lv_color_t *color_map为一个大数组，lv_color_t 被翻译为lv_color1_t，因为是单色屏幕
//每行像素数据按照显示区域的宽度依次存储，行与行之间连续排列。数据对应 area 中的矩形区域。
void ssd1680_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    lv_obj_t * act_scr = lv_scr_act();
    if(act_scr == ui_HalfmindScreen)
    {
       if(elabel_screen != HALFMIND_SCREEN)
        {
            isBaseMapFresh = false;
            elabel_screen = HALFMIND_SCREEN;
        }
        elabel_update_mode = BITMAP_UPDATE;
        ESP_LOGI(TAG,"ui_HalfmindScreen Flush called.");
    }
    else if(act_scr == ui_OTAScreen)
    {
        if(elabel_screen != OTA_SCREEN)
        {
            isBaseMapFresh = false;
            elabel_screen = OTA_SCREEN;
        }
        elabel_update_mode = FAST_UPDATE;
        ESP_LOGI(TAG,"ui_OTAScreen Flush called.");
    }
    else if(act_scr == ui_TaskScreen)
    {
        if(elabel_screen != TASK_SCREEN)
        {
            elabel_update_mode = FAST_UPDATE;
            isBaseMapFresh = false;
            elabel_screen = TASK_SCREEN;
        }
        else
        {
            elabel_update_mode = PARTIAL_UPDATE;
            elabel_partial_area = TASK_LIST;
        }
        ESP_LOGI(TAG,"ui_TaskScreen Flush called.");
    }
    else if(act_scr == ui_FocusScreen)//这里有两个screen一个是时间选择，一个是倒计时
    {
        if(elabel_partial_area == TIME_CHANGE)
        {
            if(elabel_screen != FOCUS_SCREEN)
            {
                elabel_update_mode = FAST_UPDATE;
                isBaseMapFresh = false;
                elabel_screen = FOCUS_SCREEN;
            }
            else
            {
                elabel_update_mode = PARTIAL_UPDATE;
            }
            ESP_LOGI(TAG,"ui_FocusScreen Flush called.");
        }
        else if(elabel_partial_area == TIME_SET)
        {
            if(elabel_screen != TIME_SCREEN)
            {
                elabel_update_mode = FAST_UPDATE;
                isBaseMapFresh = false;
                elabel_screen = TIME_SCREEN;
            }
            else
            {
                elabel_update_mode = PARTIAL_UPDATE;
            }
            ESP_LOGI(TAG,"ui_TimeSet Flush called.");
        }

    }
    else
    {
        ESP_LOGE(TAG,"OTHERSCREEN Flush called.");
        return;
    }

    // 打印 area 的信息
    // if (area != NULL) {
    //     ESP_LOGI("SSD1680", "Area: x1=%d, y1=%d, x2=%d, y2=%d", area->x1, area->y1, area->x2, area->y2);
    // } else {
    //     ESP_LOGI("SSD1680", "Area: NULL");
    // }

    //每个字节包含 8 个像素的数据，linelen 是字节数。我们需要覆盖显示屏的一行
    uint16_t data_num = (EPD_PANEL_WIDTH*EPD_PANEL_HEIGHT/8); 
    uint8_t *buffer = (uint8_t *) color_map;

    if(!isBaseMapFresh)
    {
        ssd1680_init();
        isBaseMapFresh = true;
        ESP_LOGI(TAG,"EPD reset");
    }
    if(elabel_update_mode == BITMAP_UPDATE)
    {
        if(elabel_bitmap_state == HALFMIND_STATE)
        {
            ESP_LOGI("SSD1680", "BITMAP HALFMIND_STATE Start Send.");
            buffer = (uint8_t *) gImage_halfmind;
            ssd1680_send_cmd(SSD1680_CMD_WRITE1_RAM);
            ssd1680_send_data(buffer,4000);
            //局刷前的basemap需要刷新这个
            // buffer = (uint8_t *) gImage_halfmind;
            // ssd1680_send_cmd(SSD1680_CMD_WRITE2_RAM);
            // ssd1680_send_data(buffer,4000);
            ssd1680_update_display();
        }
        else if(elabel_bitmap_state == QRCODE_STATE)
        {
            ESP_LOGI("SSD1680", "BITMAP QRCODE_STATE Start Send.");
            buffer = (uint8_t *) gImage_QRcode;
            ssd1680_send_cmd(SSD1680_CMD_WRITE1_RAM);
            ssd1680_send_data(buffer,4000);
            //局刷前的basemap需要刷新这个
            //buffer = (uint8_t *) gImage_QRcode;
            //ssd1680_send_cmd(SSD1680_CMD_WRITE2_RAM);
            //ssd1680_send_data(buffer,4000);
            ssd1680_update_display();
        }
    }

    else if(elabel_update_mode == FAST_UPDATE)
    {
        ESP_LOGI("SSD1680", "FAST Start Send.");
        buffer = (uint8_t *) color_map;
        ssd1680_send_cmd(SSD1680_CMD_WRITE1_RAM);
        ssd1680_send_data(buffer, data_num);
        
        //局刷前的basemap需要刷新这个
        buffer = (uint8_t *) color_map;
        ssd1680_send_cmd(SSD1680_CMD_WRITE2_RAM);
        ssd1680_send_data(buffer, data_num);

        ssd1680_update_display();
    }
    
    else if(elabel_update_mode == PARTIAL_UPDATE)
    {
        ESP_LOGI("SSD1680", "PARTIAL Start Send.");
        ssd1680_hw_reset();
        ssd1680_write_cmd(SSD1680_CMD_BWF_CTRL, ssd1680_border_part, 1);
        uint16_t x1 = 0;
        uint16_t y1 = 0;
        uint16_t x2 = 250;
        uint16_t y2 = 122;
        if(elabel_partial_area == TASK_LIST)
        {
            x1 = 0;
            y1 = 0;
            x2 = 250;
            y2 = 128;
        }
        else if(elabel_partial_area == TIME_CHANGE)
        {
            x1 = 48;
            y1 = 56;
            x2 = 200;
            y2 = 104;
        }
        else if(elabel_partial_area == TIME_SET)
        {
            x1 = 32;
            y1 = 24;
            x2 = 216;
            y2 = 104;
        }
        uint16_t X1 = y1;
        uint16_t X2 = y2;
        uint16_t Y1 = x1;
        uint16_t Y2 = x2;
        buffer = (uint8_t *) color_map;
        buffer += X1/8 + Y1*16;

        ssd1680_set_window(X1,X2,Y1,Y2);
        ssd1680_set_cursor(X1,Y1);
        
        int new_line_len = ((X2 - X1)) / 8;
        int total_rows = Y2 - Y1;
        int total_data_len = new_line_len * total_rows;

        // 分配内存以存储整个区域的数据
        uint8_t temp_buffer[total_data_len]; // 在栈上分配内存

        // 将逐行数据拷贝到 temp_buffer
        uint8_t* temp_ptr = temp_buffer;
        for (size_t row = 0; row < total_rows; row++) {
            memcpy(temp_ptr, buffer, new_line_len); // 拷贝当前行数据
            temp_ptr += new_line_len;              // 移动指针
            buffer += SSD1680_COLUMNS;             // 移动源缓冲区指针
        }

        // 一次性传输数据
        ssd1680_send_cmd(SSD1680_CMD_WRITE1_RAM);
        ssd1680_send_data(temp_buffer, total_data_len);
        ssd1680_update_display();
    }
    //通知lvgl完成传输了
    ESP_LOGI("SSD1680", "FINISH Send.\n");
    lv_disp_flush_ready(drv);
}

/* Specify the start/end positions of the window address in the X and Y
 * directions by an address unit.
 *
 * @param sx: X Start position.
 * @param ex: X End position.
 * @param ys: Y Start position.
 * @param ye: Y End position. */

static inline void ssd1680_set_window(uint16_t sx, uint16_t ex, uint16_t ys, uint16_t ye)
//(0, EPD_PANEL_WIDTH, 0, EPD_PANEL_HEIGHT)
{
    uint8_t tmp[4] = {0};
    tmp[0] = sx / 8;
    tmp[1] = ex / 8 - 1; 
    //cmd:0x44设置 RAM X - 地址开始/结束位置
    ssd1680_write_cmd(SSD1680_CMD_RAM_XPOS_CTRL, tmp, 2);

    tmp[0] = (ye-1) % 256;
    tmp[1] = (ye-1) / 256;
    tmp[2] = ys % 256;
    tmp[3] = ys / 256;
    //cmd:0x45设置 RAM Y - 地址开始/结束位置
    ssd1680_write_cmd(SSD1680_CMD_RAM_YPOS_CTRL, tmp, 4);
}

static inline void ssd1680_set_cursor(uint16_t sx, uint16_t ys)
{
    uint8_t tmp[2] = {0};

    tmp[0] = sx / 8;
    ssd1680_write_cmd(SSD1680_CMD_RAM_XPOS_CNTR, tmp, 1);

    tmp[0] = (ys) % 256;
    tmp[1] = (ys) / 256;
    ssd1680_write_cmd(SSD1680_CMD_RAM_YPOS_CNTR, tmp, 2);
}

static void ssd1680_update_display()
{
    uint8_t tmp = 0x00;
    if(elabel_update_mode == FAST_UPDATE) tmp = 0xC7;
    else if(elabel_update_mode == BITMAP_UPDATE) tmp = 0xC7;  
    else if(elabel_update_mode == FULL_UPDATE) tmp = 0xF7;
    else if(elabel_update_mode == PARTIAL_UPDATE) tmp = 0xFF;
    // ESP_LOGI(TAG,"flush with %d", tmp);
    // 0xC7;       //快刷
    // 0xF7;       //全刷
    // 0xFF;       //局刷
    ssd1680_write_cmd(SSD1680_CMD_UPDATE_CTRL2, &tmp, 1);
    ssd1680_write_cmd(SSD1680_CMD_MASTER_ACTIVATION, NULL, 0);
    ssd1680_waitbusy(SSD1680_WAIT);
}

/* Rotate the display by "software" when using PORTRAIT orientation.
 * BIT_SET(byte_index, bit_index) clears the bit_index pixel at byte_index of
 * the display buffer.
 * BIT_CLEAR(byte_index, bit_index) sets the bit_index pixel at the byte_index
 * of the display buffer. */
void ssd1680_set_px_cb(lv_disp_drv_t * disp_drv, uint8_t* buf,
    lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
    lv_color_t color, lv_opa_t opa){   
    uint16_t X,Y;
    uint32_t addr;
    uint8_t Rdata;
    X = y;
    Y = x;
    uint16_t widthByte = 16;
    addr = X/8 + Y * widthByte;
    Rdata = buf[addr];
    if(color.full == 0) {
        buf[addr] = Rdata & ~(0x80 >> (X % 8));
    } else {
        buf[addr] = Rdata | (0x80 >> (X % 8));
    }
}


void ssd1680_rounder(lv_disp_drv_t * disp_drv, lv_area_t *area)
{
    area->x1 = 0;
    area->x2 = 250 - 1;
    area->y1 = 0;
    area->y2 = 122 - 1;
}

void ssd1680_init(void)
{
    uint8_t tmp[3] = {0};
    uint8_t tmpdata = 0;

    //设置引脚模式: 调用 gpio_pad_select_gpio 函数后，指定的引脚将被配置为 GPIO 模式，允许你在后续代码中使用该引脚进行输入或输出操作。
    gpio_pad_select_gpio(SSD1680_DC_PIN);
    //设置模式为输出模式
    gpio_set_direction(SSD1680_DC_PIN, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(SSD1680_BUSY_PIN);
    //设置模式为输入模式
    gpio_set_direction(SSD1680_BUSY_PIN,  GPIO_MODE_INPUT);

#if SSD1680_USE_RST
    gpio_pad_select_gpio(SSD1680_RST_PIN);
    //设置模式为输出模式
    gpio_set_direction(SSD1680_RST_PIN, GPIO_MODE_OUTPUT);

    //硬件重置
    ssd1680_hw_reset();
#endif

    //等待2s,busy引脚变为低电平
    ssd1680_waitbusy(SSD1680_WAIT);
    
    //软件重置
    ssd1680_write_cmd(SSD1680_CMD_SW_RESET, NULL, 0);

    //等待2s,busy引脚变为低电平
    ssd1680_waitbusy(SSD1680_WAIT);

    ESP_LOGI(TAG,"fast init");
    //虽然不知道是什么原理，但是贼叼，没设置温度就是不能快刷
    //读取内部温度传感器参数
    tmpdata = 0x80;
    ssd1680_write_cmd(0x18, &tmpdata, 1);

    //Enable clock signal 
    // Load temperature value 
    // Load LUT with DISPLAY Mode 1 
    // Disable clock signal
    tmpdata = 0xB1;
    ssd1680_write_cmd(0x22, &tmpdata, 1);

    //激活所有的设置，激活完成器件busy输出高电平不因该打断，要等待
    ssd1680_write_cmd(0x20, NULL, 0);
    ssd1680_waitbusy(SSD1680_WAIT);

    //写温度参数
    tmp[0] = 0x5A;
    tmp[1] = 0x00;
    ssd1680_write_cmd(0x1A, tmp, 2);

    //Enable clock signal 
    //Load LUT with DISPLAY Mode 1 
    //Disable clock signal 
    tmpdata = 0x91;
    ssd1680_write_cmd(0x22, &tmpdata, 1);
    //激活所有的设置，激活完成器件busy输出高电平不因该打断，要等待
    ssd1680_write_cmd(0x20, NULL, 0);
    ssd1680_waitbusy(SSD1680_WAIT);

    // ESP_LOGI(TAG,"whole init");
    // //CMD:0x01驱动输出控制（Set gate driver output）
    // tmp[0] = (EPD_PANEL_HEIGHT - 1) %256;      //0x27
    // tmp[1] = (EPD_PANEL_HEIGHT - 1) /256;      //0x01
    // tmp[2] = 0x00; // GD = 0; SM = 0; TB = 0;  //0x00
    // ssd1680_write_cmd(SSD1680_CMD_GDO_CTRL, tmp, 3);

    // //CMD:0x11数据输入的方式Data Entry mode setting，Y减少，X增加，先更新X方向
    // tmpdata = 0x01;
    // ssd1680_write_cmd(SSD1680_CMD_ENTRY_MODE, &tmpdata, 1);
    
    // //CMD:0x44,0x45设置刷新的位置
    // ssd1680_set_window(0, EPD_PANEL_WIDTH, 0, EPD_PANEL_HEIGHT);
    
    // //CMD:0x3C,设置边框面板颜色，写入数据0000 0101
    // ssd1680_write_cmd(SSD1680_CMD_BWF_CTRL, ssd1680_border_init, 1);
    
    // //CMD:0x21,Display Update Control
    // tmp[0] = 0x00; //A7:0 Normal RAM content option
    // tmp[1] = 0x80; //B7 Source Output Mode: Available source from S8 to S167
    // tmp[2] = 0x00; //do not send
    // ssd1680_write_cmd(SSD1680_CMD_UPDATE_CTRL1, tmp, 2);
    
    // //CMD:0x18,读取内部温度传感器
    // tmp[0] = 0x80; //A7:0 Internal sensor
    // tmp[1] = 0x00; //do not send
    // ssd1680_write_cmd(SSD1680_CMD_READ_INT_TEMP, tmp, 1);
    
    // //CMD：0x4E，0x4F Write image data in RAM by Command
    // ssd1680_set_cursor(0, EPD_PANEL_HEIGHT);
    // ssd1680_waitbusy(SSD1680_WAIT);
}

/* Enter deep sleep mode */
void ssd1680_deep_sleep(void)
{
    uint8_t data[] = {0x01};

    /* Wait for the BUSY signal to go low */
    ssd1680_waitbusy(SSD1680_WAIT);

    ssd1680_write_cmd(SSD1680_CMD_SLEEP_MODE1, data, 1);
    vTaskDelay(100 / portTICK_RATE_MS); // 100ms delay
}

static inline void ssd1680_waitbusy(int wait_ms)
{
    int i = 0;

    vTaskDelay(10 / portTICK_RATE_MS); // 10ms delay

    for(i = 0; i < (wait_ms*10); i++) {
        if(gpio_get_level(SSD1680_BUSY_PIN) != SSD1680_BUSY_LEVEL) {
            return;
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    ESP_LOGE( TAG, "busy exceeded %dms", i*10 );

    // while(1) 
    // {
    //     if(gpio_get_level(SSD1680_BUSY_PIN) != SSD1680_BUSY_LEVEL) break;
    // }
}

//硬件reset
static inline void ssd1680_hw_reset(void)
{
    gpio_set_level(SSD1680_RST_PIN, 0);
    vTaskDelay(SSD1680_RESET_DELAY / portTICK_RATE_MS);
    gpio_set_level(SSD1680_RST_PIN, 1);
    vTaskDelay(SSD1680_RESET_DELAY / portTICK_RATE_MS);
}

/* Set DC signal to command mode */
static inline void ssd1680_command_mode(void)
{
    //D/C引脚拉低
    gpio_set_level(SSD1680_DC_PIN, 0);
}

/* Set DC signal to data mode */
static inline void ssd1680_data_mode(void)
{
    //D/C引脚拉高
    gpio_set_level(SSD1680_DC_PIN, 1);
}

static inline void ssd1680_write_cmd(uint8_t cmd, uint8_t *data, size_t len)
{
    //先传输一帧cmd再传输一帧data
    disp_wait_for_pending_transactions();
    ssd1680_command_mode();
    disp_spi_send_data(&cmd, 1);

    if (data != NULL) {
	ssd1680_data_mode();
	disp_spi_send_data(data, len);
    }
}

/* Send cmd to the display */
static inline void ssd1680_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    ssd1680_command_mode();
    //传输spi消息（quene模式），不需要告诉lvgl传输完成
    disp_spi_send_data(&cmd, 1);
}

/* Send length bytes of data to the display */
static inline void ssd1680_send_data(uint8_t *data, uint16_t length)
{
    //等待所有被塞入队列的spi事件处理完
    disp_wait_for_pending_transactions();
    ssd1680_data_mode();
    //传输spi消息（quene模式），且告诉lvgl传输完成
    disp_spi_send_data(data, length);
}

