#include "NoHostState.hpp"
#include "global_message.h"
#include "Esp_now_client.hpp"

void no_host_change_button_choice()
{
    if(NoHostState::Instance()->no_host_process != No_host_disconnecting_host_process) return;
    ESP_LOGI("NoHostState", "change_button_choice");
    NoHostState::Instance()->button_host_active_choose_left = !NoHostState::Instance()->button_host_active_choose_left;
    NoHostState::Instance()->need_flash_paper = true;
}

void no_host_confirm_button_choice()
{
    if(NoHostState::Instance()->no_host_process != No_host_disconnecting_host_process) return;
    //如果是cancel则退回到activeState
    if(NoHostState::Instance()->button_host_active_choose_left)
    {
        NoHostState::Instance()->need_back = true;
    }
    //如果是retry重新进入这个状态机
    else
    {
        NoHostState::Instance()->enter_connect_host();
    }
}

void NoHostState::Init(ElabelController* pOwner)
{
}

void NoHostState::Enter(ElabelController* pOwner)
{
    EspNowClient::Instance()->is_connect_to_host = false;
    enter_connect_host();
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(no_host_change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(no_host_change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(no_host_confirm_button_choice);

    ESP_LOGI(STATEMACHINE,"Enter NoHostState.");
}

void NoHostState::Execute(ElabelController* pOwner)
{
    if(no_host_process == No_host_connecting_host_process)
    {
        if(EspNowClient::Instance()->is_connect_to_host) enter_success_connect_host();
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
            enter_disconnect_host();
        }
    }
    else if(no_host_process == No_host_disconnecting_host_process)
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
            release_lvgl();           
        }
    }
}


void NoHostState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(no_host_change_button_choice);
    ControlDriver::Instance()->button7.CallbackLongPress.unregisterCallback(no_host_change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(no_host_confirm_button_choice);
    //等待2s蓝牙完全清理
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(STATEMACHINE,"Out NoHostState.\n");
}