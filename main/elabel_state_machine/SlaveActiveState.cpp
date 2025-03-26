#include "SlaveActiveState.hpp"
#include "Esp_now_client.hpp"
#include "network.h"
#include "control_driver.hpp"
#include "Esp_now_slave.hpp"

void change_slave_active_button_choice()
{
    ESP_LOGI("SlaveActiveState", "change_button_choice");
    SlaveActiveState::Instance()->button_slave_active_confirm_left = !SlaveActiveState::Instance()->button_slave_active_confirm_left;
    SlaveActiveState::Instance()->need_flash_paper = true;
}

void confirm_slave_active_button_choice()
{
    ESP_LOGI("SlaveActiveState", "confirm_slave_active_button_choice");
    //如果是在测试连接状态
    if(SlaveActiveState::Instance()->need_test_connect)
    {
        SlaveActiveState::Instance()->need_forward = true;
    }
    //如果是在确定激活者的状态
    else
    {
        if(SlaveActiveState::Instance()->button_slave_active_confirm_left)
        {
            SlaveActiveState::Instance()->need_back = true;
        }
        else
        {
            SlaveActiveState::Instance()->enter_test_connect();
        }
    }
}

void SlaveActiveState::Init(ElabelController* pOwner)
{
}

void SlaveActiveState::Enter(ElabelController* pOwner)
{
    //停止蓝牙激活
    stop_blue_activate();

    button_slave_active_confirm_left = false;
    need_test_connect = false;
    need_back = false;
    need_forward = false;
    need_flash_paper = false;

    enter_connect_host();
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ESP_LOGI(STATEMACHINE,"Enter SlaveActiveState.");
}

void SlaveActiveState::Execute(ElabelController* pOwner)
{
    if(need_test_connect)
    {
        if(elabelUpdateTick%20 == 0)
        {
            //发送espnow消息来测试连接
            uint8_t message = 0x00;
            EspNowClient::Instance()->send_esp_now_message(EspNowSlave::Instance()->host_mac, &message, 1, default_message_type, true, 0);
        }  

        if(elabelUpdateTick%2000 == 0)
        { 
            ESP_LOGI("SlaveActiveState", "recieve packet count: %d", EspNowClient::Instance()->m_recieve_packet_count);
            if(EspNowClient::Instance()->m_recieve_packet_count > 100) EspNowClient::Instance()->m_recieve_packet_count = 100;
            lock_lvgl();
            char buffer[32];
            sprintf(buffer, "Score: %d", EspNowClient::Instance()->m_recieve_packet_count);
            set_text_without_change_font(ui_ConnectGuide2, buffer);
            release_lvgl();
            EspNowClient::Instance()->m_recieve_packet_count = 0;
        }
    }
    else
    {   
        if(need_flash_paper)
        {
            lock_lvgl();
            //重新刷新按钮
            if(button_slave_active_confirm_left)
            {
                lv_obj_add_state(ui_SlaveActiveCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_SlaveActiveConfirm, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_SlaveActiveCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_SlaveActiveConfirm, LV_STATE_PRESSED );
            }
            set_text_without_change_font(ui_Username, EspNowSlave::Instance()->username);
            release_lvgl();
        }
    }
}   

void SlaveActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}