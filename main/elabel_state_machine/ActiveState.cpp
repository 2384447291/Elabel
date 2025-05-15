#include "ActiveState.hpp"
#include "network.h"
#include "http.h"
#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"
#include "Esp_now_client.hpp"

void change_guide_page(uint8_t page)
{
    lock_lvgl();
    lv_obj_add_flag(ui_Activate1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Activate2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Activate3, LV_OBJ_FLAG_HIDDEN);
    
    switch(page)    
    {
        case 0:
            lv_obj_clear_flag(ui_Activate1, LV_OBJ_FLAG_HIDDEN);
            break;
        case 1:
            lv_obj_clear_flag(ui_Activate2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 2:
            lv_obj_clear_flag(ui_Activate3, LV_OBJ_FLAG_HIDDEN);
            break;
        default:
            break;
    }
    
    release_lvgl();
}

void move_to_next_page()
{
    ActiveState::Instance()->m_active_page = (ActiveState::Instance()->m_active_page + 1) % 3;
    change_guide_page(ActiveState::Instance()->m_active_page);
}

void move_to_previous_page()
{
    ActiveState::Instance()->m_active_page = (ActiveState::Instance()->m_active_page - 1 + 3) % 3;
    change_guide_page(ActiveState::Instance()->m_active_page);
}

void ActiveState::Init(ElabelController* pOwner)
{
    
}

void ActiveState::Enter(ElabelController* pOwner)
{
    m_active_page = 0;

    //将两个连接指标置为false
    set_is_connect_to_host(false);
    set_is_connect_to_phone(false);

    EspNowHost::Instance()->deinit();
    EspNowSlave::Instance()->deinit();

    //启动蓝牙激活任务用来激活主机
    start_blue_activate();
    //启动espnow激活任务用来激活从机
    EspNowClient::Instance()->start_find_channel();
    
    lock_lvgl();
    switch_screen(ui_ActiveScreen);
    release_lvgl();

    change_guide_page(m_active_page);

    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(move_to_previous_page);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(move_to_next_page);

    ESP_LOGI(STATEMACHINE,"Enter ActiveState.");
}

void ActiveState::Execute(ElabelController* pOwner)
{
}

void ActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(move_to_previous_page);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(move_to_next_page);
    ESP_LOGI(STATEMACHINE,"Out ActiveState.\n");
}