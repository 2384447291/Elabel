#include "HostActiveState.hpp"
#include "network.h"
#include "http.h"
#include "Esp_now_client.hpp"

void host_out_active_state()
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
    EspNowHost::Instance()->init();
    Out_ActiveState = false;
    lock_lvgl();
    switch_screen(ui_HostActiveScreen);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter HostActiveState.");
}

void HostActiveState::Execute(ElabelController* pOwner)
{
    //当wifi连接成功后，开始注册
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
        ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(host_out_active_state);
        ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(host_out_active_state);
    }

    //当注册完成后开始广播espnow信息
    if(get_wifi_status() == 0x03)
    {
        //注册退出事件
        if(elabelUpdateTick % 100 == 0)
        {
            EspNowHost::Instance()->send_broadcast_message();
        }

        if(painted_slave_num != EspNowHost::Instance()->slave_num)
        {
            lock_lvgl();
            repaint_para();
            release_lvgl();
        }
    }
}

void HostActiveState::Exit(ElabelController* pOwner)
{
    stop_blue_activate();
    ESP_LOGI(STATEMACHINE,"Out HostActiveState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(host_out_active_state);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(host_out_active_state);
}