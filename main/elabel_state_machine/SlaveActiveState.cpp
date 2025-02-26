#include "SlaveActiveState.hpp"
#include "Esp_now_client.hpp"
#include "network.h"

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
    stop_blue_activate();
    EspNowSlave::Instance()->init();
    Out_ActiveState = false;
    lock_lvgl();
    switch_screen(ui_SlaveActiveScreen);
    repaint_para();
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter SlaveActiveState.");
    ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(slave_out_active_state);
    ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(slave_out_active_state);
}

void SlaveActiveState::Execute(ElabelController* pOwner)
{
    if(elabelUpdateTick % 100 == 0)
    {
        EspNowSlave::Instance()->send_feedback_message();
    }
}

void SlaveActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(slave_out_active_state);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(slave_out_active_state);
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}