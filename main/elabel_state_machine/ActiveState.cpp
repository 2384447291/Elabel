#include "ActiveState.hpp"
#include "network.h"
#include "http.h"
#include "Esp_now_client.hpp"

void change_language()
{   
    get_global_data()->m_language = (language)((get_global_data()->m_language + 1) % 2);
    set_nvs_info_uint8_t("language",(uint8_t)(get_global_data()->m_language));
    Change_All_language();
}

void ActiveState::Init(ElabelController* pOwner)
{
    
}

void ActiveState::Enter(ElabelController* pOwner)
{
    //启动更新信道任务
    EspNowClient::Instance()->start_find_channel();
    //将两个连接指标置为false
    set_is_connect_to_host(false);
    set_is_connect_to_phone(false);

    //反初始化espnow，防止后续的重新初始化
    EspNowHost::Instance()->deinit(); 
    EspNowSlave::Instance()->deinit();

    //启动蓝牙激活任务
    start_blue_activate();

    lock_lvgl();
    lv_scr_load(ui_ActiveScreen);
    release_lvgl();

    ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(change_language);
    ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(change_language);
    ESP_LOGI(STATEMACHINE,"Enter ActiveState.");
}

void ActiveState::Execute(ElabelController* pOwner)
{
}

void ActiveState::Exit(ElabelController* pOwner)
{
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    ESP_LOGI(STATEMACHINE,"Out ActiveState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(change_language);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(change_language);
}