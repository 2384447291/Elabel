#include "NowifiState.hpp"
#include "network.h"
#include "ui.h"
#include "ssd1680.h"

void NoWifiState::Init(ElabelController* pOwner)
{
    
}

void NoWifiState::Enter(ElabelController* pOwner)
{
    set_led_state(device_no_wifi);
    set_bit_map_state(QRCODE_STATE);
    lock_lvgl();
    lv_scr_load(ui_HalfmindScreen);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter NoWifiState.");
}

void NoWifiState::Execute(ElabelController* pOwner)
{

}

void NoWifiState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out NowifiState.\n");
}