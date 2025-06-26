#include "SleepFocusState.hpp"
#include "control_driver.hpp"
#include "global_time.h"
#include "global_draw.h"
#include "FocusTaskState.hpp"
void SleepFocusState::Init(ElabelController* pOwner)
{
}

void SleepFocusState::Enter(ElabelController* pOwner)
{
    //同步时间戳
    EspNowSlave::Instance()->slave_send_espnow_http_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));

    int focus_time =  FocusTaskState::Instance()->choose_task_fall_timing - (get_unix_time() - FocusTaskState::Instance()->choose_task_start_time)/1000;
    ESP_LOGI(STATEMACHINE,"Enter SleepFocusState,Time countdown: %d", focus_time);
    next_wake_up_time = 0;
    need_out_state = false;    

    lock_lvgl();
    switch_screen(ui_SleepFocusScreen);
    //设置倒计时任务长度
    set_text_without_change_font(ui_SleepTaskFocusName, FocusTaskState::Instance()->choose_task_title);
    
    //设置倒计时
    int inner_time_countdown_s = get_inner_countdown_time();
    char timestr[20];
    if(inner_time_countdown_s >= 0)
    {
        sprintf(timestr, "< %02d:%02d", inner_time_countdown_s / 60, inner_time_countdown_s % 60);
    }
    else
    {
        sprintf(timestr, "> %02d:%02d", (-inner_time_countdown_s) / 60, (-inner_time_countdown_s) % 60);
    }
    set_text_without_change_font(ui_SleepTaskFocusTime, timestr);

    //设置进度条
    int process_length = 0;
    if(inner_time_countdown_s >= 0)
    {
        process_length =  round(360.0f *( 1 - (float)(inner_time_countdown_s)/(float)(FocusTaskState::Instance()->choose_task_fall_timing)));
        lv_arc_set_value(ui_MinuteBar, process_length);
    }
    else
    {
        process_length =  360.0f;
    }
    lv_arc_set_value(ui_CoutdownBar, process_length);
    release_lvgl();

    //等待2s墨水瓶刷新
    vTaskDelay(pdMS_TO_TICKS(WAITING_RESUME_TIME));

    //关闭其他额外线程
    ControlDriver::Instance()->stop_button_check_task();
    suspend_gui();

    //进入睡眠
    EspNowSlave::Instance()->slave_send_espnow_http_sleep_request();
    EspNowSlave::Instance()->sleep_sync_flag = 0;
    start_sleep();
}
void SleepFocusState::Execute(ElabelController* pOwner)
{
    
}

void SleepFocusState::Exit(ElabelController* pOwner)
{
    BatteryManager::Instance()->setPowerState(true);
    ESP_ERROR_CHECK(esp_wifi_start());
    EspNowSlave::Instance()->resume_espnow();

    ControlDriver::Instance()->start_button_check_task();
    resume_gui();
    vTaskDelay(pdMS_TO_TICKS(500));
}

