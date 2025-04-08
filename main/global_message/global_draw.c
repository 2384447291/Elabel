#include "global_draw.h"
#include "global_message.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../../components/ui/ui.h"
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

int16_t ChooseTask_big_height = 32;
int16_t ChooseTask_big_width = 209;
int16_t ChooseTask_small_height = 32;
int16_t ChooseTask_small_width = 192;

uint8_t get_button_size(Button_type button_type)
{
    switch(button_type)
    {
        case Btn_ChooseTask_big_height:
            return ChooseTask_big_height;
        case Btn_ChooseTask_big_width:
            return ChooseTask_big_width;
        case Btn_ChooseTask_small_height:
            return ChooseTask_small_height;
        case Btn_ChooseTask_small_width:
            return ChooseTask_small_width;
        default:
            return 0;
    }
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
    // lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),MALLOC_CAP_DMA); 
    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),MALLOC_CAP_SPIRAM); 
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
// const lv_font_t* get_language_font(bool Is_bigger)
// {
//     switch(get_global_data()->m_language) {
//         case Chinese:
//             if(Is_bigger) return &ui_font_Chinese_28;
//             else return &ui_font_Chinese_20;
//         case English:
//         default:
//             if(Is_bigger) return &lv_font_montserrat_20;
//             else return &lv_font_montserrat_16;
//     }
// }
//--------------------------------------获取语言字体-------------------------------------//


//--------------------------------------修改任务内容-------------------------------------//
void lvgl_modify_task(int position, const char *task_content) 
{
    lv_obj_t *ui_tmpButton = lv_obj_get_child(ui_TaskContainer, position);
    lv_obj_t *ui_Label1 = lv_obj_get_child(ui_tmpButton, 0);
    //获取文本长度
    size_t len = strlen(task_content);
    //如果超过13个字符,截断并添加省略号
    char truncated[14];
    if(len > 13) {
        strncpy(truncated, task_content, 13);
        truncated[10] = '.';
        truncated[11] = '.';
        truncated[12] = '.';
        truncated[13] = '\0';
        task_content = truncated;
    }
    set_text_without_change_font(ui_Label1, task_content);
    ESP_LOGI("LVGL","任务%d \"%s\" 修改成功！\n", position, task_content);
}
//--------------------------------------修改任务内容-------------------------------------//


//--------------------------------------更新任务列表-------------------------------------//
//chose_task: 从0开始表示第一个task在中间
void update_lvgl_task_list(int center_task)
{
    int task_num = get_global_data()->m_todo_list->size;
    if(task_num > 0)
    {
        //如果有事件，则显示任务列表,默认任务列表有两个空节点
        _ui_flag_modify(ui_NoTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_HaveTaskContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);   
        lv_obj_clear_flag(ui_Button1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Button3, LV_OBJ_FLAG_HIDDEN);
        lvgl_modify_task(1, get_global_data()->m_todo_list->items[center_task].title);
        if(task_num - center_task - 1 == 0) 
        {
            lv_obj_add_flag(ui_Button3, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lvgl_modify_task(2, get_global_data()->m_todo_list->items[center_task + 1].title);
            
        }

        if(center_task == 0)
        {
            lv_obj_add_flag(ui_Button1, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lvgl_modify_task(0, get_global_data()->m_todo_list->items[center_task - 1].title);
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
// void set_text_with_change_font(lv_obj_t * target_label,  const char * text, bool Is_bigger)
// {
//     //当你设置样式时，LV_PART_MAIN 允许你定义对象的主要视觉特征，例如背景颜色、边框、字体等
//     //LV_STATE_DEFAULT 是指对象在没有任何特殊状态时的外观
//     lv_label_set_text(target_label, text);
//     lv_obj_set_style_text_font(target_label, get_language_font(Is_bigger), LV_PART_MAIN | LV_STATE_DEFAULT);
//     uint8_t child_count = lv_obj_get_child_cnt(target_label);
//     for(int i = 0; i < child_count; i++)
//     {
//         lv_obj_t *ui_tmpLabel = lv_obj_get_child(target_label, i);
//         lv_label_set_text(ui_tmpLabel, text);
//         lv_obj_set_style_text_font(ui_tmpLabel, get_language_font(Is_bigger), LV_PART_MAIN | LV_STATE_DEFAULT);
//     }
// }


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


//--------------------------------------更改所有语言------------------------------------//
void Change_All_language()
{
    if(get_global_data()->m_language == 0)
    {
    }
    else if(get_global_data()->m_language == 1)
    {
    }
}
//--------------------------------------更改所有语言------------------------------------//

void switch_screen(lv_obj_t* new_screen) {
    // lv_obj_set_parent(ui_LED, new_screen);
    set_force_full_update(true);
    lv_scr_load(new_screen);
}