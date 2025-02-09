#include "HostActiveState.hpp"
#include "network.h"
#include "http.h"

void out_active_state()
{
    if(strlen(get_global_data()->m_usertoken) == 0) ESP_LOGI("HostActiveState","User token is empty");
    else HostActiveState::Instance()->Out_ActiveState = true;
}

void HostActiveState::Init(ElabelController* pOwner)
{
    Out_ActiveState = false;
}

void HostActiveState::Enter(ElabelController* pOwner)
{
    Out_ActiveState = false;
    lock_lvgl();
    lv_scr_load(ui_HostActiveScreen);
    release_lvgl();
    ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(out_active_state);
    ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(out_active_state);
    ESP_LOGI(STATEMACHINE,"Enter ActiveState.");
}

void HostActiveState::Execute(ElabelController* pOwner)
{
    if(get_wifi_status()==0x02)
    {
        //等待500ms连接稳定和token接收
        vTaskDelay(500 / portTICK_PERIOD_MS);
        http_find_usr(true);
        http_bind_user(true);
        //设定为已经绑定了设备
        set_nvs_info("username",get_global_data()->m_userName);                  
        lock_lvgl();
        repaint_para();
        release_lvgl();
        set_wifi_status(0x03);
    }
}

void HostActiveState::Exit(ElabelController* pOwner)
{
    stop_blue_activate();
    //如果状态不是0x00，不会连接的
    m_wifi_connect();
    ESP_LOGI(STATEMACHINE,"Out ActiveState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(out_active_state);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(out_active_state);
}