#include "ActiveState.hpp"
#include "network.h"
#include "http.h"
#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"

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
    //将两个连接指标置为false
    set_is_connect_to_host(false);
    set_is_connect_to_phone(false);

    //反初始化espnow，防止后续的重新初始化
    EspNowHost::Instance()->deinit(); 
    EspNowSlave::Instance()->deinit();

    //启动蓝牙激活任务用来激活主机
    start_blue_activate();
    //启动更新信道任务
    EspNowClient::Instance()->start_find_channel();
    
    lock_lvgl();
    switch_screen(ui_ActiveScreen);
    release_lvgl();

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
}