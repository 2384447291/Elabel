#include "SleepState.hpp"
#include "control_driver.hpp"
#include "global_time.h"
#include "global_draw.h"
#include "control_driver.hpp"
#include "global_draw.h"

void SleepState::Init(ElabelController* pOwner)
{
}

void SleepState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter SleepState.");
    next_wake_up_time = 0;
    need_out_state = false;
    start_sleep_time = 0;
    need_clock_mode = get_global_data()->m_device_info.is_idel_clock_time;
    //同步时间戳
    EspNowSlave::Instance()->slave_send_espnow_http_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));

    //如果需要显示idel_clock,则切换界面
    if(need_clock_mode)
    {
        lock_lvgl();
        switch_screen(ui_SleepClockScreen);
        //设置todo的数目
        char str_todo_num[20];
        sprintf(str_todo_num, "%d to-dos", get_global_data()->m_todo_list->size);
        set_text_without_change_font(ui_TodoNum, str_todo_num);
        //设置date
        time_description date_time = get_date_time();
        char str_date[20];
        sprintf(str_date, "%s %s", month_abbr[date_time.month], data_abbr[date_time.day - 1]);
        set_text_without_change_font(ui_Date, str_date);
        //设置hour
        char str_hour[20];
        sprintf(str_hour, "%s", time_abbr[date_time.hour]);
        set_text_without_change_font(ui_Hour, str_hour);
        //设置minutes
        int minute_length =  round(6.0f * (float)(date_time.minute+1));
        lv_arc_set_value(ui_MinuteBar, minute_length);
        release_lvgl();
    }
    //把所有choosetask的内容改为静止的
    else
    {
        lock_lvgl();
        uint8_t child_count = lv_obj_get_child_cnt(ui_TaskContainer);
        for(int i = 0; i<child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(ui_TaskContainer, i);
            lv_obj_t *ui_Label1 = lv_obj_get_child(child, 0);
            lv_label_set_long_mode(ui_Label1, LV_LABEL_LONG_DOT);
            for(int j = 0; j < lv_obj_get_child_cnt(ui_Label1); j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(ui_Label1, j);
                lv_label_set_long_mode(label_child, LV_LABEL_LONG_DOT);
            }
        }
        release_lvgl();
    }
    //等待2s页面刷新
    vTaskDelay(pdMS_TO_TICKS(WAITING_RESUME_TIME));

    //关闭其他额外线程
    ControlDriver::Instance()->stop_button_check_task();
    suspend_gui();

    EspNowSlave::Instance()->slave_send_espnow_http_sleep_request();
    EspNowSlave::Instance()->sleep_sync_flag = 0;
    start_sleep();
}
void SleepState::Execute(ElabelController* pOwner)
{
    
}

void SleepState::Exit(ElabelController* pOwner)
{
    BatteryManager::Instance()->setPowerState(true);
    ESP_ERROR_CHECK(esp_wifi_start());
    EspNowSlave::Instance()->resume_espnow();

    ControlDriver::Instance()->start_button_check_task();
    resume_gui();
    vTaskDelay(pdMS_TO_TICKS(500));
}

