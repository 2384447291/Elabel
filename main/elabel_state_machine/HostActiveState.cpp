#include "HostActiveState.hpp"
#include "network.h"
#include "http.h"
#include "esp_now_host.hpp"
#include "global_message.h"

void change_button_choice()
{
    if(!HostActiveState::Instance()->is_fail) return;
    ESP_LOGI("HostActiveState", "change_button_choice");
    HostActiveState::Instance()->button_choose_left = !HostActiveState::Instance()->button_choose_left;
    lock_lvgl();
    if(HostActiveState::Instance()->button_choose_left)
    {
        lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );
        lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
    }
    else
    {
        lv_obj_clear_state(ui_HostActiveCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_HostActiveRetry, LV_STATE_PRESSED );
    }
    //改这个属性不会单独触发刷新
    set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
    release_lvgl();
}

void confirm_button_choice()
{
    //如果是cancel则退回到activeState
    if(HostActiveState::Instance()->button_choose_left)
    {
        HostActiveState::Instance()->need_back = true;
    }
    //如果是retry重新进入这个状态机
    else
    {
        HostActiveState::Instance()->Enter(ElabelController::Instance());
    }
}

void HostActiveState::Init(ElabelController* pOwner)
{
}

void HostActiveState::Enter(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_button_choice);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_button_choice);

    button_choose_left = true;
    is_fail = false;
    is_set_wifi = false;
    need_back = false;
    need_forward = false;
    reconnect_count_down = RECONNECT_COUNT_DOWN;

    lock_lvgl();
    switch_screen(ui_HostActiveScreen);

    lv_obj_add_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
    lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );

    set_text_without_change_font(ui_WIFIname, "Waiting...");
    set_text_without_change_font(ui_Disconnectwifiname, "Waiting...");
    set_text_without_change_font(ui_HostActiveAutoTime, "Timeout in 30 secs");

    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter HostActiveState.");
}

void HostActiveState::Execute(ElabelController* pOwner)
{
    if(is_fail) return;
    //如果正在连接wifi
    if(get_wifi_status() == 0x01)
    {
        if(is_set_wifi)
        {
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
                is_fail = true;
                //禁止重新连接的断连
                m_wifi_disconnect();
                button_choose_left = true;
                lock_lvgl();
                switch_screen(ui_HostActiveScreen);
                lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );
                release_lvgl();
                return;
            }
        }
        else
        {
            is_set_wifi = true;
            lock_lvgl();
            switch_screen(ui_HostActiveScreen);
            set_text_without_change_font(ui_WIFIname, get_global_data()->m_wifi_ssid);
            set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
            reconnect_count_down = RECONNECT_COUNT_DOWN;
            release_lvgl();
        }
    }
    //当wifi连接成功后，开始注册
    if(get_wifi_status()==0x02)
    {
        lock_lvgl();
        set_text_without_change_font(ui_HostActiveAutoTime, "Success!!!");
        release_lvgl();
        //等待500ms连接稳定和token接收
        vTaskDelay(500 / portTICK_PERIOD_MS);
        http_find_usr(true);
        http_bind_user(true);
        //保存连接记录
        set_nvs_info("username",get_global_data()->m_userName);
        //完成激活标志                  
        need_forward = true;
        set_wifi_status(0x03);
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