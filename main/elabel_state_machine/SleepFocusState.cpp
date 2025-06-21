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
    uint32_t focus_time =  FocusTaskState::Instance()->choose_task_fall_timing - (get_unix_time() - FocusTaskState::Instance()->choose_task_start_time)/1000;
    ESP_LOGI(STATEMACHINE,"Enter SleepFocusState,Time countdown: %d", focus_time);
    next_wake_up_time = 0;
    need_out_state = false;    

    //同步时间戳
    EspNowSlave::Instance()->slave_send_espnow_http_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));

    //关闭其他额外线程
    ControlDriver::Instance()->stop_button_check_task();

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
    vTaskDelay(pdMS_TO_TICKS(500));
}

