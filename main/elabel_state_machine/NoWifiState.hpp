#ifndef NOWIFISTATE_HPP
#define NOWIFISTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "network.h"
#define RECONNECT_COUNT_DOWN 30

typedef enum
{
    default_No_wifi_process,
    No_wifi_connecting_wifi_process,
    No_wifi_disconnecting_wifi_process,
    No_wifi_success_connect_wifi_process,
} No_wifi_process;

class NoWifiState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    No_wifi_process no_wifi_process = default_No_wifi_process;

    bool button_host_active_choose_left = true;
    bool need_back = false;
    bool need_forward = false;
    bool need_flash_paper = false;
    uint8_t reconnect_count_down = RECONNECT_COUNT_DOWN;

    void enter_connect_wifi()
    {
        m_wifi_disconnect();
        m_wifi_connect();
        need_forward = false;
        need_back = false;
        need_flash_paper = false;

        reconnect_count_down = RECONNECT_COUNT_DOWN;
        no_wifi_process = No_wifi_connecting_wifi_process;
        lock_lvgl();
        lv_obj_clear_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_HostActiveAutoTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HostActiveAutoTimePanding, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HostActivateSuccess, LV_OBJ_FLAG_HIDDEN);

        set_text_without_change_font(ui_WIFIname, get_global_data()->m_wifi_ssid);
        char time_str[40];
        sprintf(time_str, "%d", reconnect_count_down);
        set_text_without_change_font(ui_HostActiveAutoTime, time_str);
        release_lvgl();
    }

    void enter_disconnect_wifi()
    {
        need_forward = false;
        no_wifi_process = No_wifi_disconnecting_wifi_process;
        //禁止重新连接的断连
        m_wifi_disconnect();
        need_back = false;
        need_flash_paper = false;

        lock_lvgl();

        button_host_active_choose_left = true;
        lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );

        set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
        
        lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        release_lvgl();
    }

    void enter_success_connect_wifi()
    {
        no_wifi_process = No_wifi_success_connect_wifi_process;
        lock_lvgl();

        lv_obj_add_flag(ui_HostActiveAutoTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HostActiveAutoTimePanding, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HostActivateSuccess, LV_OBJ_FLAG_HIDDEN);

        release_lvgl();
        need_forward = true;
    }

    static NoWifiState* Instance()
    {
        static NoWifiState instance;
        return &instance;
    }
};
#endif