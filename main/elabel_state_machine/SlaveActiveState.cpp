#include "SlaveActiveState.hpp"
#include "Esp_now_client.hpp"
#include "network.h"
#include "control_driver.hpp"

void slave_out_active_state()
{
    SlaveActiveState::Instance()->Out_ActiveState = true;
}


void SlaveActiveState::Init(ElabelController* pOwner)
{
    Out_ActiveState = false;
}

void SlaveActiveState::Enter(ElabelController* pOwner)
{
    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(slave_out_active_state);
    stop_blue_activate();
    EspNowSlave::Instance()->init();
    Out_ActiveState = false;
    lock_lvgl();
    switch_screen(ui_SlaveActiveScreen);
    repaint_para();
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter SlaveActiveState.");
    //开始回传绑定消息
    EspNowSlave::Instance()->send_bind_message();
}

void SlaveActiveState::Execute(ElabelController* pOwner)
{
}

void SlaveActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(slave_out_active_state);
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}