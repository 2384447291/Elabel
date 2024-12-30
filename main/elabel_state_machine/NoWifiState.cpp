#include "NowifiState.hpp"
#include "network.h"
#include "http.h"

void out_activate()
{
    if(get_global_data()->usertoken != NULL 
    && get_global_data()->wifi_ssid != NULL
    && get_global_data()->wifi_password != NULL)
    {
        NoWifiState::Instance()->need_out_activate = true;
    }
    else
    {
        ESP_LOGI(STATEMACHINE,"No need to out activate.");
    }
}

void NoWifiState::Init(ElabelController* pOwner)
{
    
}

void NoWifiState::Enter(ElabelController* pOwner)
{
    ControlDriver::Instance()->getLED().setLedState(LedState::DEVICE_NO_WIFI);
    need_out_activate = false;
    start_activate();
    lock_lvgl();
    lv_scr_load(ui_BindScreen);
    repaint_para();
    release_lvgl();
    ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(out_activate);
    ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(out_activate);
    ESP_LOGI(STATEMACHINE,"Enter NoWifiState.");
}

void NoWifiState::Execute(ElabelController* pOwner)
{
    if(get_wifi_status()==0x02)
    {
        //等待500ms连接稳定和token接收
        vTaskDelay(500 / portTICK_PERIOD_MS);
        http_find_usr(true);
        http_bind_user(true);
        //设定为已经绑定了设备
        nvs_handle wificfg_nvs_handler;
        ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );
        ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"username",get_global_data()->userName));
        ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
        nvs_close(wificfg_nvs_handler);                     /* 关闭 */ 
        lock_lvgl();
        repaint_para();
        release_lvgl();
        set_wifi_status(0x03);
        ControlDriver::Instance()->getLED().setLedState(LedState::NO_LIGHT);
    }
}

void NoWifiState::Exit(ElabelController* pOwner)
{
    stop_activate();
    //如果状态不是0x00，不会连接的
    m_wifi_connect();
    ESP_LOGI(STATEMACHINE,"Out NowifiState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(out_activate);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(out_activate);
}