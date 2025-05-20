#include "SleepState.hpp"
#include "control_driver.hpp"
#include "global_time.h"
#include "global_draw.h"
void SleepState::Init(ElabelController* pOwner)
{
}

void SleepState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter SleepState.");
    need_out_state = false;
    char clock_time[6];
    get_clock_time(clock_time);
    lock_lvgl();
    switch_screen(ui_SleepScreen);
    set_text_without_change_font(ui_SleepCLock, clock_time);
    memcpy(show_clock_time, clock_time, 6);
    release_lvgl();
    EspNowSlave::Instance()->slave_send_espnow_http_sleep_request();
    //等待2s页面刷新
    vTaskDelay(pdMS_TO_TICKS(2000));
    start_sleep_time = esp_timer_get_time();
    enter_sleep();
}
void SleepState::Execute(ElabelController* pOwner)
{
    
}

void SleepState::Exit(ElabelController* pOwner)
{
}

