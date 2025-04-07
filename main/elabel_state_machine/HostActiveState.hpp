#ifndef HOSTACTIVESTATE_HPP
#define HOSTACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_host.hpp"
#include "http.h"
#define RECONNECT_COUNT_DOWN 30

typedef enum
{
    default_Hostactive_process,
    Hostactive_waiting_wifi_process,
    Hostactive_connecting_wifi_process,
    Hostactive_disconnecting_wifi_process,
    Hostactive_success_connect_wifi_process,
} Host_Active_process;

class HostActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    Host_Active_process host_active_process = default_Hostactive_process;

    bool button_host_active_choose_left = true;
    bool need_back = false;
    bool need_forward = false;
    bool need_flash_paper = false;
    uint8_t reconnect_count_down = RECONNECT_COUNT_DOWN;

    void enter_waiting_wifi()
    {
        need_forward = false;
        need_back = false;
        host_active_process = Hostactive_waiting_wifi_process;
        reconnect_count_down = RECONNECT_COUNT_DOWN;

        lock_lvgl();
        switch_screen(ui_HostActiveScreen);

        lv_obj_clear_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );

        set_text_without_change_font(ui_WIFIname, "Waiting...");
        set_text_without_change_font(ui_Disconnectwifiname, "Waiting...");
        set_text_without_change_font(ui_HostActiveAutoTime, "Timeout in 30 secs");

        release_lvgl();
    }

    void enter_connect_wifi()
    {
        host_active_process = Hostactive_connecting_wifi_process;
        lock_lvgl();
        set_text_without_change_font(ui_WIFIname, get_global_data()->m_wifi_ssid);
        set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
        reconnect_count_down = RECONNECT_COUNT_DOWN;
        release_lvgl();
    }

    void enter_disconnect_wifi()
    {
        host_active_process = Hostactive_disconnecting_wifi_process;
        //禁止重新连接的断连
        m_wifi_disconnect();
        need_back = false;
        button_host_active_choose_left = true;
        need_flash_paper = false;

        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);
        lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        release_lvgl();
    }

    void enter_success_connect_wifi()
    {
        host_active_process = Hostactive_success_connect_wifi_process;
        lock_lvgl();
        set_text_without_change_font(ui_HostActiveAutoTime, "Success!!!");
        release_lvgl();
        //等待500ms连接稳定和token接收
        vTaskDelay(500 / portTICK_PERIOD_MS);
        http_find_usr(true);
        http_bind_user(true);
        //保存user的激活记录
        set_nvs_info("username",get_global_data()->m_userName);
        //保存主机标志
        get_global_data()->m_is_host = 1;
        set_nvs_info_uint8_t_array("is_host",&get_global_data()->m_is_host,1);
        //完成激活标志                  
        need_forward = true;
    }

    static HostActiveState* Instance()
    {
        static HostActiveState instance;
        return &instance;
    }
};
#endif