#ifndef NOHOSTSTATE_HPP
#define NOHOSTSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Esp_now_slave.hpp"
#include "battery_manager.hpp"
#include "esp_timer.h"
#define RECONNECT_COUNT_DOWN 30
#define ENTER_SLEEP_COUNT_DOWN 10

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
    uint8_t enter_sleep_count_down = ENTER_SLEEP_COUNT_DOWN;

    void enter_connect_host()
    {
        EspNowClient::Instance()->is_connect_to_host = false;
        EspNowClient::Instance()->start_find_channel();
        no_host_process = No_host_connecting_host_process;
        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        lv_obj_add_flag(ui_HostActiveGuide, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_SlaveActiveGuide, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_HostActiveAutoTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HostActiveAutoTimePanding, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HostActivateSuccess, LV_OBJ_FLAG_HIDDEN);

        char mac_str[18];
        uint8_t* mac = get_global_data()->m_host_mac;
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        set_text_without_change_font(ui_WIFIname, mac_str);
        set_text_without_change_font(ui_HostActiveAutoTime, "30");
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
        enter_sleep_count_down = ENTER_SLEEP_COUNT_DOWN;

        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        lv_obj_add_flag(ui_HostActiveGuide, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_SlaveActiveGuide, LV_OBJ_FLAG_HIDDEN);
        char mac_str[18];
        uint8_t* mac = get_global_data()->m_host_mac;
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        set_text_without_change_font(ui_Disconnectwifiname, mac_str);
        lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );
        release_lvgl();
    }

    void enter_success_connect_host()
    {
        //停止寻找信道
        EspNowClient::Instance()->stop_find_channel();
        //稳妥起见,再设置一次wifi_channel，怕停止信道后被操作了
        uint8_t actual_wifi_channel = 0;
        wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
        esp_wifi_set_channel(get_global_data()->m_host_channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_get_channel(&actual_wifi_channel, &wifi_second_channel);
        ESP_LOGI(ESP_NOW, "Get Host, set espnow channel to %d", actual_wifi_channel);
        no_host_process = No_host_success_connect_host_process;
        lock_lvgl();

        lv_obj_add_flag(ui_HostActiveAutoTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_HostActiveAutoTimePanding, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_HostActivateSuccess, LV_OBJ_FLAG_HIDDEN);

        release_lvgl();
        need_forward = true;
    }

    void enter_sleep()
    {
        //关闭其他额外线程
        ControlDriver::Instance()->stop_button_check_task();
        suspend_gui();

        printf("Enter Sleep\n");
        //关闭wifi
        ESP_ERROR_CHECK(esp_wifi_stop());
        //关闭外设电源
        BatteryManager::Instance()->setPowerState(false);
        ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
        uint64_t mask = (1ULL << DEVICE_BUTTON_1234) | (1ULL << DEVICE_BUTTON_5678);  
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH));  // 任意引脚高电平触发唤醒
        esp_light_sleep_start();

        BatteryManager::Instance()->setPowerState(true);
        ESP_ERROR_CHECK(esp_wifi_start());
        EspNowSlave::Instance()->resume_espnow();

        ControlDriver::Instance()->start_button_check_task();
        resume_gui();
        vTaskDelay(pdMS_TO_TICKS(500));
        //重新全刷界面
        enter_disconnect_host();
    }

    static NoHostState* Instance()
    {
        static NoHostState instance;
        return &instance;
    }
};
#endif