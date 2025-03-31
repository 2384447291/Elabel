#include "HostActiveState.hpp"
#include "network.h"
#include "http.h"
#include "esp_now_host.hpp"
#include "Esp_now_client.hpp"
#include "global_message.h"

void change_button_choice()
{
    if(HostActiveState::Instance()->host_active_process != Hostactive_disconnecting_wifi_process) return;
    ESP_LOGI("HostActiveState", "change_button_choice");
    HostActiveState::Instance()->button_host_active_choose_left = !HostActiveState::Instance()->button_host_active_choose_left;
    HostActiveState::Instance()->need_flash_paper = true;
}

void confirm_button_choice()
{
    if(HostActiveState::Instance()->host_active_process != Hostactive_disconnecting_wifi_process) return;
    //如果是cancel则退回到activeState
    if(HostActiveState::Instance()->button_host_active_choose_left)
    {
        HostActiveState::Instance()->need_back = true;
    }
    //如果是retry重新进入这个状态机
    else
    {
        HostActiveState::Instance()->enter_waiting_wifi();
    }
}

void HostActiveState::Init(ElabelController* pOwner)
{
}

void HostActiveState::Enter(ElabelController* pOwner)
{
    //停止寻找频道
    EspNowClient::Instance()->stop_find_channel();
    enter_waiting_wifi();
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_button_choice);

    ESP_LOGI(STATEMACHINE,"Enter HostActiveState.");
}

void HostActiveState::Execute(ElabelController* pOwner)
{
    if(host_active_process == Hostactive_waiting_wifi_process)
    {
        if(get_wifi_status() == 0x01)
        {
            enter_connect_wifi();
        }
    }
    else if(host_active_process == Hostactive_connecting_wifi_process)
    {
        if(get_wifi_status() == 0x02) enter_success_connect_wifi();
        if(elabelUpdateTick%1000 == 0)
        {
            reconnect_count_down--;
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
    else if(host_active_process == Hostactive_disconnecting_wifi_process)
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
        }
    }
}


void HostActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_button_choice);
    ControlDriver::Instance()->button7.CallbackLongPress.unregisterCallback(change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_button_choice);
    stop_blue_activate();
    //等待2s蓝牙完全清理
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(STATEMACHINE,"Out HostActiveState.\n");
}