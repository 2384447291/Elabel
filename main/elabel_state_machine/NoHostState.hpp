#ifndef NOHOSTSTATE_HPP
#define NOHOSTSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Esp_now_slave.hpp"
#define RECONNECT_COUNT_DOWN 30

typedef enum
{
    default_No_host_process,
    No_host_connecting_host_process,
    No_host_disconnecting_host_process,
    No_host_success_connect_host_process,
} No_host_process;

class NoHostState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);



    No_host_process no_host_process = default_No_host_process;

    bool button_host_active_choose_left = true;
    bool need_back = false;
    bool need_forward = false;
    bool need_flash_paper = false;
    uint8_t reconnect_count_down = RECONNECT_COUNT_DOWN;

    void enter_connect_host()
    {
        EspNowClient::Instance()->start_find_channel();
        no_host_process = No_host_connecting_host_process;
        lock_lvgl();
        set_text_without_change_font(ui_HostActiveGuide1, "Connect to HOST");

        char mac_str[18];
        uint8_t* mac = get_global_data()->m_host_mac;
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        set_text_without_change_font(ui_WIFIname, mac_str);
        set_text_without_change_font(ui_Disconnectwifiname, "Timeout in 30 secs");
        reconnect_count_down = RECONNECT_COUNT_DOWN;
        release_lvgl();
    }

    void enter_disconnect_host()
    {
        EspNowClient::Instance()->stop_find_channel();
        no_host_process = No_host_disconnecting_host_process;
        need_back = false;
        button_host_active_choose_left = true;
        need_flash_paper = false;

        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        char mac_str[18];
        uint8_t* mac = get_global_data()->m_host_mac;
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        set_text_without_change_font(ui_Disconnectwifiname, mac_str);
        lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        release_lvgl();
    }

    void enter_success_connect_host()
    {
        EspNowClient::Instance()->stop_find_channel();
        no_host_process = No_host_success_connect_host_process;
        lock_lvgl();
        set_text_without_change_font(ui_HostActiveAutoTime, "Success!!!");
        release_lvgl();
        need_forward = true;
    }

    static NoHostState* Instance()
    {
        static NoHostState instance;
        return &instance;
    }
};
#endif