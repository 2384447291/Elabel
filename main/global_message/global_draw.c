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
#include "esp_timer.h"

#define TAG "LVGL"
#undef ESP_LOGI
#define ESP_LOGI(tag, format, ...) 
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

bool is_lvgl_sleep = false;

//--------------------------------------lvgl主任务-------------------------------------//
/* 创建一个 semaphore 来处理对 lvgl 的并发调用。
* 如果你想从其他线程/任务中调用 *any* lvgl 函数
* 则应锁定同一个信号！*/
void periodic_timer_callback(void *arg) {
    (void)arg;
    lv_tick_inc(1);
}

esp_timer_handle_t periodic_timer = NULL;

void start_lvgl_tick_timer(void)
{
    if (periodic_timer == NULL) {
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = periodic_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "periodic_gui",
            .skip_unhandled_events = false
        };
        ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    }
    if(!esp_timer_is_active(periodic_timer))
    {
        ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));
    }
}

void stop_lvgl_tick_timer(void)
{
    if (periodic_timer != NULL && esp_timer_is_active(periodic_timer)) {
        ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
    }
}

//--------------------------------------lvgl主任务-------------------------------------//
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
    start_lvgl_tick_timer();
    //--------------创建并启动一个定时器，用于定期调用 lv_tick_task 函数(每隔 1 毫秒触发一次),来告诉lvgl过了1ms--------------//


    //--------------开始lvgl线程--------------//
    ESP_LOGI("lvgl", "start gui task.\n");
    ui_init();
    lv_refr_now(NULL);
    is_lvgl_sleep = false;
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


TaskHandle_t gui_task_handle = NULL;

void Gui_init()
{
    if(gui_task_handle == NULL)
    {
        xTaskCreate(guiTask, "gui", 8 * 1024, NULL, 0, &gui_task_handle);
    }
    else
    {
        ESP_LOGE("gui", "gui task already exists");
    }
}

void suspend_gui()
{
    if(is_lvgl_sleep) return;
    stop_lvgl_tick_timer();
    vTaskSuspend(gui_task_handle);
    is_lvgl_sleep = true;
}

void resume_gui()
{
    if(!is_lvgl_sleep) return;
    start_lvgl_tick_timer();
    vTaskResume(gui_task_handle);
    is_lvgl_sleep = false;
}
//--------------------------------------lvgl主任务-------------------------------------//

//--------------------------------------修改任务内容-------------------------------------//
void lvgl_modify_task(int position, const char *task_content) 
{
    lv_obj_t *ui_tmpButton = lv_obj_get_child(ui_TaskContainer, position);
    lv_obj_t *ui_Label1 = lv_obj_get_child(ui_tmpButton, 0);
    set_text_without_change_font(ui_Label1, task_content);
    ESP_LOGI("LVGL","任务%d \"%s\" 修改成功！\n", position, task_content);
}

//--------------------------------------修改任务内容-------------------------------------//


//--------------------------------------更新任务列表-------------------------------------//
//chose_task: 从0开始表示第一个task在中间
void update_lvgl_task_list(int center_task, uint8_t guide_page)
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
        lv_obj_clear_flag(ui_Notask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NotaskTip1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NotaskTip2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NotaskTip3, LV_OBJ_FLAG_HIDDEN);
        // if(guide_page == 0)
        // {   
        //     lv_obj_clear_flag(ui_Notask, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip1, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip2, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip3, LV_OBJ_FLAG_HIDDEN);
        // }
        // else if(guide_page == 1)
        // {
        //     lv_obj_add_flag(ui_Notask, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_clear_flag(ui_NotaskTip1, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip2, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip3, LV_OBJ_FLAG_HIDDEN);

        // }
        // else if(guide_page == 2)
        // {   
        //     lv_obj_add_flag(ui_Notask, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip1, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_clear_flag(ui_NotaskTip2, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip3, LV_OBJ_FLAG_HIDDEN);
        // }
        // else if(guide_page == 3)
        // {
        //     lv_obj_add_flag(ui_Notask, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip1, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_add_flag(ui_NotaskTip2, LV_OBJ_FLAG_HIDDEN);
        //     lv_obj_clear_flag(ui_NotaskTip3, LV_OBJ_FLAG_HIDDEN);
        // }
    }
}
//--------------------------------------更新任务列表-------------------------------------//


uint16_t lv_label_count_lines_wrap(lv_obj_t * label, char* first_title)
{
    if(label == NULL) {
        ESP_LOGI(TAG, "label is NULL");
        return 0;
    }

    /* 可选：打印 long_mode，确认 wrap 已生效 */
    lv_label_long_mode_t lm = lv_label_get_long_mode(label);
    ESP_LOGI(TAG, "当前 long_mode = %d (LV_LABEL_LONG_WRAP = %d)", lm, LV_LABEL_LONG_WRAP);

    const char * txt = lv_label_get_text(label);
    if(txt == NULL) {
        ESP_LOGI(TAG, "文本指针为 NULL");
        return 0;
    }
    ESP_LOGI(TAG, "文本内容: \"%s\"", txt);

    const lv_font_t * font = lv_obj_get_style_text_font(label, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_coord_t line_space   = lv_obj_get_style_text_line_space(label, LV_PART_MAIN | LV_STATE_DEFAULT);
    ESP_LOGI(TAG, "字体指针: %p, letter_space=%d, line_space=%d", font, letter_space, line_space);
    lv_obj_update_layout(label);
    lv_coord_t total_w = lv_obj_get_width(label);
    ESP_LOGI(TAG, "对象总宽度 total_w=%d", total_w);
    if(total_w <= 0) {
        ESP_LOGI(TAG, "total_w <= 0，返回 0");
        return 0;
    }
    lv_coord_t pad_left  = lv_obj_get_style_pad_left(label,  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_coord_t pad_right = lv_obj_get_style_pad_right(label, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_coord_t box_width = total_w - pad_left - pad_right;
    ESP_LOGI(TAG, "pad_left=%d, pad_right=%d, 可用宽度 box_width=%d", pad_left, pad_right, box_width);
    if(box_width <= 0) {
        ESP_LOGI(TAG, "box_width <= 0，返回 0");
        return 0;
    }

    uint16_t line_count = 0;
    const char * p = txt;
    size_t offset = 0;
    size_t copied = 0;

    while(*p) {
        line_count++;
        ESP_LOGI(TAG, "---- 第 %u 行 ----", line_count);
        ESP_LOGI(TAG, " offset=%zu, 内容开始: \"%s\"", offset, p);

        /* 新签名：传 NULL 给 used_width，flag 传 0 */
        uint32_t adv = _lv_txt_get_next_line(p, font, letter_space, box_width, NULL, 0);
        ESP_LOGI(TAG, "_lv_txt_get_next_line adv=%lu", adv);

        //打印log
        if(adv > 0) {
            uint16_t len = adv;
            const uint16_t MAX_PRINT = 128;
            if(len > MAX_PRINT) len = MAX_PRINT;
            char buf[MAX_PRINT + 1];
            memcpy(buf, p, len);
            buf[len] = '\0';
            ESP_LOGI(TAG, "第 %u 行内容（%u bytes）: \"%s\"%s",
                     line_count, len, buf, (adv > MAX_PRINT) ? "..." : "");
        } else {
            ESP_LOGI(TAG, "最后一行剩余内容: \"%s\"", p);
            break;
        }
        //打印log

        if(first_title!=NULL)
        {
            if(line_count <= 2) 
            {
                memcpy(first_title + copied, p, adv);
                copied += adv;
                first_title[copied] = '\0';
            }
            ESP_LOGI(TAG, "first_title=%s", first_title);
        }

        p += adv;
        offset += adv;
    }   

    ESP_LOGI(TAG, "统计到总行数: %u", line_count);

    /* 可选：打印预估高度 vs 对象实际高度 */
    lv_coord_t font_h = lv_font_get_line_height(font);
    lv_coord_t est_h = font_h * line_count + line_space * (line_count - 1)
                       + lv_obj_get_style_pad_top(label, LV_PART_MAIN | LV_STATE_DEFAULT)
                       + lv_obj_get_style_pad_bottom(label, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_coord_t real_h = lv_obj_get_height(label);
    ESP_LOGI(TAG, "预估渲染高度: %d, 对象实际高度: %d", est_h, real_h);

    return line_count;
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

void switch_screen(lv_obj_t* new_screen) {
    // lv_obj_set_parent(ui_LED, new_screen);
    set_force_full_update(true);
    lv_scr_load(new_screen);
}


