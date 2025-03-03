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
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}