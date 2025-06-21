#include "OtaPrepareState.hpp"
#include "network.h"
#include "http.h"
#include "global_message.h"
#include "Esp_now_slave.hpp"

void ota_prepare_change_button_choice()
{
    if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_disconnecting_wifi_process ||
       OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_no_need_ota_process)
       {
            ESP_LOGI("OtaPrepareState", "change_button_choice");
            OtaPrepareState::Instance()->button_ota_prepare_choose_left = !OtaPrepareState::Instance()->button_ota_prepare_choose_left;
            OtaPrepareState::Instance()->need_flash_paper = true;
       }
}

void ota_prepare_change_next_need_ota_screen()
{
    if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_need_to_ota_process)
    {
        OtaPrepareState::Instance()->current_step++;
        if(OtaPrepareState::Instance()->current_step == OtaPrepareState::Instance()->all_step + 2)
        {
            OtaPrepareState::Instance()->current_step = OtaPrepareState::Instance()->all_step + 1;
        }
        else
        {
            OtaPrepareState::Instance()->need_flash_paper = true;
        }
    }
}

void ota_prepare_change_previous_need_ota_screen()
{
    if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_need_to_ota_process)
    {
        OtaPrepareState::Instance()->current_step--;
        if(OtaPrepareState::Instance()->current_step == -1)
        {
            OtaPrepareState::Instance()->current_step = 0;
        }
        else
        {
            OtaPrepareState::Instance()->need_flash_paper = true;
        }
    }
}

void ota_prepare_confirm_button_choice()
{
    if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_disconnecting_wifi_process)
    {
        //如果是cancel则退reboot
        if(OtaPrepareState::Instance()->button_ota_prepare_choose_left)
        {
            OtaPrepareState::Instance()->enter_ota_finish(false);
        }
        //如果是retry重新进入这个状态机
        else
        {
            OtaPrepareState::Instance()->enter_connect_wifi();
        }
    }
    else if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_no_need_ota_process)
    {
        //如果是cancel则退reboot
        if(OtaPrepareState::Instance()->button_ota_prepare_choose_left)
        {
            OtaPrepareState::Instance()->enter_ota_finish(false);
        }
        //如果是retry重新进入这个状态机
        else
        {
            OtaPrepareState::Instance()->enter_decide_ota();
        }
    }
    else if(OtaPrepareState::Instance()->m_ota_prepare_process == ota_prepare_need_to_ota_process)
    {
        //如果是cancel则退reboot
        if(OtaPrepareState::Instance()->current_step == OtaPrepareState::Instance()->all_step)
        {
            OtaPrepareState::Instance()->enter_ota_finish(false);
        }
        //如果是start则开始ota
        else if(OtaPrepareState::Instance()->current_step == OtaPrepareState::Instance()->all_step + 1)
        {
            OtaPrepareState::Instance()->enter_ota_finish(true);
        }
    }
}

void OtaPrepareState::Init(ElabelController* pOwner)
{
}

void OtaPrepareState::Enter(ElabelController* pOwner)
{
    //进入这个状态机只有两条路重启或者开始ota
    button_ota_prepare_choose_left = true;
    need_flash_paper = false;
    need_enter_ota = false;
    need_out_ota_prepare = false;
    
    ota_prepare_wait_tick = OTA_WAIT_TICK;
    reconnect_count_down = RECONNECT_COUNT_DOWN;

    //如果是从机
    if(get_global_data()->m_is_host == 2)
    {    
        http_client_init();
        //获取最新的wifi信息
        if(EspNowSlave::Instance()->slave_send_espnow_http_get_wifi_info() == ESP_OK)
        {
            //等待1s获取返回的wifi——info
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        //获取最新的user token
        if(EspNowSlave::Instance()->slave_send_espnow_http_get_user_token() == ESP_OK)
        {
            //等待1s获取返回的user token
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    if(get_wifi_status() != 0x02)
    {
        enter_connect_wifi();
    }
    else
    {
        enter_decide_ota();
    }
    
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(ota_prepare_change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(ota_prepare_change_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(ota_prepare_change_previous_need_ota_screen);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(ota_prepare_change_next_need_ota_screen);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(ota_prepare_confirm_button_choice);

    ESP_LOGI(STATEMACHINE,"Enter OtaPrepareState.");
}

void OtaPrepareState::Execute(ElabelController* pOwner)
{
    if(m_ota_prepare_process == ota_prepare_connecting_wifi_process)
    {
        if(get_wifi_status() == 0x02) enter_success_connect_wifi();
        if(elabelUpdateTick%5000 == 0)
        {
            reconnect_count_down-=5;
            lock_lvgl();
            char time_str[40];
            sprintf(time_str, "Timeout in %d secs", reconnect_count_down);
            set_text_without_change_font(ui_HostActiveAutoTime, time_str);
            release_lvgl();
        }
        if(reconnect_count_down == 0)
        {
            enter_disconnect_wifi();
        }
    }
    else if(m_ota_prepare_process == ota_prepare_disconnecting_wifi_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            //重新刷新按钮
            if(button_ota_prepare_choose_left)
            {
                lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_HostActiveCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_HostActiveRetry, LV_STATE_PRESSED );
            }
            set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
            release_lvgl(); 
            need_flash_paper = false;          
        }
    }
    else if(m_ota_prepare_process == ota_prepare_no_need_ota_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            //重新刷新按钮
            if(button_ota_prepare_choose_left)
            {
                lv_obj_add_state(ui_RetryCheckVersionButtonCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_RetryCheckVersionButtonRetry, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_RetryCheckVersionButtonCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_RetryCheckVersionButtonRetry, LV_STATE_PRESSED );
            }
            set_text_without_change_font(ui_NewFirmware, "Fail Get Update Data");
            release_lvgl(); 
            need_flash_paper = false;          
        }        
    }
    else if(m_ota_prepare_process == ota_prepare_need_to_ota_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            ota_prepare_wait_tick--;
            if(ota_prepare_wait_tick <= 0)
            {
                enter_ota_finish(false);
                return;
            }
        }

        if(need_flash_paper)
        {
            lock_lvgl();
            update_ota_describtion();
            release_lvgl(); 
            need_flash_paper = false;          
        }
    }
}


void OtaPrepareState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(ota_prepare_change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(ota_prepare_change_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(ota_prepare_change_previous_need_ota_screen);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(ota_prepare_change_next_need_ota_screen);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(ota_prepare_confirm_button_choice);
    ESP_LOGI(STATEMACHINE,"Out OtaPrepareState.\n");
}