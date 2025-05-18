#include "NoWifiState.hpp"
#include "network.h"
#include "http.h"
#include "global_message.h"
#include "Esp_now_client.hpp"

void no_wifi_change_button_choice()
{
    if(NoWifiState::Instance()->no_wifi_process != No_wifi_disconnecting_wifi_process) return;
    ESP_LOGI("NoWifiState", "change_button_choice");
    NoWifiState::Instance()->button_host_active_choose_left = !NoWifiState::Instance()->button_host_active_choose_left;
    NoWifiState::Instance()->need_flash_paper = true;
}

void no_wifi_confirm_button_choice()
{
    if(NoWifiState::Instance()->no_wifi_process != No_wifi_disconnecting_wifi_process) return;
    //如果是cancel则退回到activeState
    if(NoWifiState::Instance()->button_host_active_choose_left)
    {
        NoWifiState::Instance()->need_back = true;
    }
    //如果是retry重新进入这个状态机
    else
    {
        NoWifiState::Instance()->enter_waiting_wifi();
    }
}

void NoWifiState::Init(ElabelController* pOwner)
{
}

void NoWifiState::Enter(ElabelController* pOwner)
{
    m_wifi_connect();
    enter_waiting_wifi();
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(no_wifi_change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(no_wifi_change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(no_wifi_confirm_button_choice);

    ESP_LOGI(STATEMACHINE,"Enter NoWifiState.");
}

void NoWifiState::Execute(ElabelController* pOwner)
{
    if(no_wifi_process == No_wifi_waiting_wifi_process)
    {
        if(get_wifi_status() == 0x01)
        {
            enter_connect_wifi();
        }
    }
    else if(no_wifi_process == No_wifi_connecting_wifi_process)
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
    else if(no_wifi_process == No_wifi_disconnecting_wifi_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            //重新刷新按钮
            if(button_host_active_choose_left)
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
}


void NoWifiState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(no_wifi_change_button_choice);
    ControlDriver::Instance()->button7.CallbackLongPress.unregisterCallback(no_wifi_change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(no_wifi_confirm_button_choice);
    stop_blue_activate();
    //等待2s蓝牙完全清理
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(STATEMACHINE,"Out NoWifiState.\n");
}