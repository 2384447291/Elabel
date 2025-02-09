#include "global_draw.h"
#include "global_message.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ui.h"
#include "lvgl_helpers.h"
#include "ssd1680.h"
//--------------------------------------lvgl相关的内容-------------------------------------//
SemaphoreHandle_t xGuiSemaphore;
void lock_lvgl()
{
    while(true)
    {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) break;
    }
}
void release_lvgl()
{
    xSemaphoreGive(xGuiSemaphore);
}

void Inituilock()
{
    xGuiSemaphore = xSemaphoreCreateMutex();
}
//--------------------------------------lvgl相关的内容-------------------------------------//



//--------------------------------------lvgl主任务-------------------------------------//
/* 创建一个 semaphore 来处理对 lvgl 的并发调用。
* 如果你想从其他线程/任务中调用 *any* lvgl 函数
* 则应锁定同一个信号！*/
void periodic_timer_callback(void *arg) {
    (void)arg;
    lv_tick_inc(1);
}

void guiTask(void *pvParameter) {
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
      .callback = periodic_timer_callback,
        .arg = NULL,  // 替换 nullptr 为 NULL
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
//--------------------------------------lvgl主任务-------------------------------------//



//--------------------------------------获取语言字体-------------------------------------//
const lv_font_t* get_language_font(void)
{
    switch(get_global_data()->m_language) {
        case Chinese:
            return &ui_font_Chinese_20;
        case English:
        default:
            return &lv_font_montserrat_20;
    }
}
//--------------------------------------获取语言字体-------------------------------------//


//--------------------------------------修改任务内容-------------------------------------//
void lvgl_modify_task(int position, const char *task_content) 
{
    uint8_t child_count = lv_obj_get_child_cnt(ui_HaveTaskContainer);
    ESP_LOGI("Task_list", "Child count is %d.\n", child_count);
    lv_obj_t *ui_tmpButton = lv_obj_get_child(ui_HaveTaskContainer, position+1);
    lv_obj_t *ui_Label1 = lv_obj_get_child(ui_tmpButton, 0);
    set_text(ui_Label1, task_content);
    ESP_LOGI("LVGL","任务%d \"%s\" 修改成功！\n", position, task_content);
}
//--------------------------------------修改任务内容-------------------------------------//


//--------------------------------------添加任务到链表末尾-------------------------------------//
void lvgl_add_task(const char *task_content) 
{
    //ui_Container3是存储了task_list的容器
    uint8_t child_count = lv_obj_get_child_cnt(ui_HaveTaskContainer);
    ESP_LOGI("Task_list", "Child count is %d.\n", child_count);
    //取出最后一个元素，并删除，0开始计数所以要减1
    lv_obj_t *empty_label = lv_obj_get_child(ui_HaveTaskContainer, child_count-1);
    if(empty_label != NULL && empty_label!= 0)
    {
        lv_obj_del(empty_label);
    }

    //新建Button承接task
    lv_obj_t *ui_tmpButton = lv_btn_create(ui_HaveTaskContainer);
    lv_obj_set_width(ui_tmpButton, 130);
    lv_obj_set_height(ui_tmpButton, 24);
    lv_obj_set_align(ui_tmpButton, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_tmpButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_tmpButton, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_tmpButton, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_tmpButton, lv_color_hex(0xF6F6F6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_tmpButton, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_tmpButton, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_tmpButton, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_tmpButton, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    //构建第一个挂载button下面的第一个label
    lv_obj_t *ui_Label1 = lv_label_create(ui_tmpButton);
    lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_Label1, get_language_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_Label1, task_content);
    lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(ui_Label1, 0);
    lv_obj_set_y(ui_Label1, 0);

    //给新建的button赋具体的文字，5遍是为了加粗
    for(int i = 0; i < 1; i++)
    {
        lv_obj_t *ui_tmpLabel = lv_label_create(ui_Label1);
        lv_obj_set_width(ui_tmpLabel, LV_SIZE_CONTENT);     /// 1
        lv_obj_set_height(ui_tmpLabel, LV_SIZE_CONTENT);    /// 1
        lv_obj_set_align(ui_tmpLabel, LV_ALIGN_CENTER);
        lv_obj_set_style_text_font(ui_tmpLabel, get_language_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_tmpLabel, task_content);
        lv_obj_set_style_text_color(ui_tmpLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_tmpLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        if(i == 0)
        {
            lv_obj_set_x(ui_tmpLabel, -1);
            lv_obj_set_y(ui_tmpLabel, 0);
        }
    }

    //给末尾附上新的空button填补
    ui_endlabel = lv_label_create(ui_HaveTaskContainer);
    lv_obj_set_width(ui_endlabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_endlabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_endlabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_endlabel, "\n");

    ESP_LOGI("LVGL","任务 \"%s\" 添加成功！\n", task_content);
}
//--------------------------------------添加任务到链表末尾-------------------------------------//



//--------------------------------------删除指定位置的任务-------------------------------------//
void lvgl_delete_task(int position) 
{
    // 删除头结点
    if (position == 0) 
    {
        //由于空头节点的出现，所以要删除第二个
        lv_obj_t *ui_tmpButton = lv_obj_get_child(ui_HaveTaskContainer, position+1);
        lv_obj_del(ui_tmpButton);
        return;
    }

    lv_obj_t *ui_tmpButton = lv_obj_get_child(ui_HaveTaskContainer, position+1);
    lv_obj_del(ui_tmpButton);
}
//--------------------------------------删除指定位置的任务-------------------------------------//



//--------------------------------------更新任务列表-------------------------------------//
void update_lvgl_task_list()
{
    if(get_global_data()->m_todo_list->size > 0)
    {
        //如果有事件，则显示任务列表,默认任务列表有两个空节点
        _ui_flag_modify(ui_NoTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_HaveTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);   
        uint8_t child_count = lv_obj_get_child_cnt(ui_HaveTaskContainer);
        for(int i = 0; i < (child_count - 2 - get_global_data()->m_todo_list->size); i++)
        {
            lvgl_delete_task(i);
        }
        child_count = lv_obj_get_child_cnt(ui_HaveTaskContainer);
        for(int i = 0; i<get_global_data()->m_todo_list->size; i++)
        {
            if(child_count > 2 )
            {
                lvgl_modify_task(i, get_global_data()->m_todo_list->items[i].title); 
                child_count --;
            }
            else
            {
                lvgl_add_task(get_global_data()->m_todo_list->items[i].title);
            }
        }
    }
    else
    {
        //如果没有事件，则显示no task enjoy life
        _ui_flag_modify(ui_HaveTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_NoTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}
//--------------------------------------更新任务列表-------------------------------------//



//--------------------------------------修改label-------------------------------------//
void set_text(lv_obj_t * target_label,  const char * text)
{
    lv_obj_set_style_text_font(target_label, get_language_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(target_label, text);
    uint8_t child_count = lv_obj_get_child_cnt(target_label);
    for(int i = 0; i < child_count; i++)
    {
        lv_obj_t *ui_tmpLabel = lv_obj_get_child(target_label, i);
        lv_obj_set_style_text_font(ui_tmpLabel, get_language_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_tmpLabel, text);
    }
}


//--------------------------------------修改label-------------------------------------//
void set_text_without_change_font(lv_obj_t * target_label,  const char * text)
{
    lv_label_set_text(target_label, text);
    uint8_t child_count = lv_obj_get_child_cnt(target_label);
    for(int i = 0; i < child_count; i++)
    {
        lv_obj_t *ui_tmpLabel = lv_obj_get_child(target_label, i);
        lv_label_set_text(ui_tmpLabel, text);
    }
}
//--------------------------------------修改label-------------------------------------//


//--------------------------------------更改字体为中文-------------------------------------//
void set_text_chinese(lv_obj_t * target_label,  const char * text)
{
    //当你设置样式时，LV_PART_MAIN 允许你定义对象的主要视觉特征，例如背景颜色、边框、字体等
    //LV_STATE_DEFAULT 是指对象在没有任何特殊状态时的外观
    lv_label_set_text(target_label, text);
    lv_obj_set_style_text_font(target_label, &ui_font_Chinese_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    uint8_t child_count = lv_obj_get_child_cnt(target_label);
    for(int i = 0; i < child_count; i++)
    {
        lv_obj_t *ui_tmpLabel = lv_obj_get_child(target_label, i);
        lv_label_set_text(ui_tmpLabel, text);
        lv_obj_set_style_text_font(ui_tmpLabel, &ui_font_Chinese_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
//--------------------------------------更改字体为中文-------------------------------------//


//--------------------------------------更改字体为英文------------------------------------//
void set_text_english(lv_obj_t * target_label,  const char * text)
{
    //当你设置样式时，LV_PART_MAIN 允许你定义对象的主要视觉特征，例如背景颜色、边框、字体等
    //LV_STATE_DEFAULT 是指对象在没有任何特殊状态时的外观
    lv_label_set_text(target_label, text);
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    uint8_t child_count = lv_obj_get_child_cnt(target_label);
    for(int i = 0; i < child_count; i++)
    {
        lv_obj_t *ui_tmpLabel = lv_obj_get_child(target_label, i);
        lv_label_set_text(ui_tmpLabel, text);
        lv_obj_set_style_text_font(ui_tmpLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
//--------------------------------------更改字体为英文------------------------------------//


//--------------------------------------更改所有语言------------------------------------//
void Change_All_language()
{
    if(get_global_data()->m_language == 0)
    {
        set_text_english(ui_ActiveGuide, "Use APP to active Host\n"
                                        "or\n"
                                        "Use Host to active Slave");
        set_text_english(ui_HostName, "Host");
        set_text_english(ui_SlaveName, "Slave");
        set_text_english(ui_HostInf, "Username:\n"
                                    "WIFIname:\n"
                                    "WIFIPassword:\n"
                                    "SlaveNum:");
        set_text_english(ui_SlaveInf, "Username:\n"
                                    "HostMac:\n");
        set_text_english(ui_NewFirmware, "New Firmware");
        set_text_english(ui_OperateGuide ,"Click: Apply\n"
                                        "Rotate: Skip");
        set_text_english(ui_Updating, "Updating...");
        set_text_english(ui_FocusTask, "Focus Task");
        set_text_english(ui_ShutdownGuide, "See you next time");
        set_text_english(ui_NoTaskNote, "Nothing left yet");
        set_text_english(ui_NoTaskNote2, "Enjoy your life");
    }
    else if(get_global_data()->m_language == 1)
    {
        set_text_chinese(ui_ActiveGuide, "打开手机APP靠近唤醒主机\n"
                                        "或\n"
                                        "使用激活主机靠近唤醒从机");
        set_text_chinese(ui_HostName, "主机");
        set_text_chinese(ui_SlaveName, "从机");
        set_text_chinese(ui_HostInf, "用户名:\n"
                                    "WiFi名称:\n"
                                    "WiFi密码:\n"
                                    "从机数量:");
        set_text_chinese(ui_SlaveInf, "用户名:\n"
                                    "主机MAC:");
        set_text_chinese(ui_NewFirmware, "新固件");
        set_text_chinese(ui_OperateGuide, "单点: 应用\n"
                                        "旋转: 跳过");
        set_text_chinese(ui_Updating, "正在更新...");
        set_text_chinese(ui_FocusTask, "专注任务");
        set_text_chinese(ui_ShutdownGuide, "明天再见");
        set_text_chinese(ui_NoTaskNote, "没有剩余任务");
        set_text_chinese(ui_NoTaskNote2, "享受你的生活");
    }
}
    //--------------------------------------更改所有语言------------------------------------//
