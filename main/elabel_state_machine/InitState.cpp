#include "InitState.hpp"
#include "http.h"
#include "m_mqtt.h"
#include "control_driver.hpp"
#include "network.h"
#include "Esp_now_client.hpp"
#include "Esp_now_slave.hpp"
#include "Esp_now_host.hpp"
void Confirm_ota_button_state()
{
    if(InitState::Instance()->button_choose_ota_left)
    {
        InitState::Instance()->is_need_ota = 2;
    }
    else
    {
        InitState::Instance()->is_need_ota = 1;
    }
}

void change_ota_button_state()
{
    if(InitState::Instance()->is_need_ota != 0) return;
    InitState::Instance()->ota_Wait_tick = 300;
    InitState::Instance()->button_choose_ota_left = !InitState::Instance()->button_choose_ota_left;
    lock_lvgl();
    lv_obj_clear_flag(ui_OTAButton, LV_OBJ_FLAG_HIDDEN);
    if(InitState::Instance()->button_choose_ota_left)
    {
        lv_obj_add_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_OTAButtonStart, LV_STATE_PRESSED );
    }
    else
    {
        lv_obj_clear_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_OTAButtonStart, LV_STATE_PRESSED );
    }
    release_lvgl();
}


void InitState::Init(ElabelController* pOwner)
{
    
}

void InitState::Enter(ElabelController* pOwner)
{
    //连接wifi
    if(get_global_data()->m_is_host == 1) m_wifi_connect();
    is_init = false;
    is_need_ota = 0;
    lock_lvgl();
    switch_screen(ui_HalfmindScreen);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter InitState.");
}

void InitState::Execute(ElabelController* pOwner)
{
    //主机的初始化流程
    if(get_global_data()->m_is_host == 1)
    {
        //如果正在连接或者没有用户，则不进行初始化
        if(get_wifi_status() == 1 && get_global_data()->m_usertoken!=NULL) return;
        //如果已经初始化或者需要OTA则不进行初始化
        if(is_init || is_need_ota == 1) return;

        //等待1swifi连接两秒稳定
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //时间同步(堵塞等待)
        HTTP_syset_time();
        //获取挂墙时间
        get_unix_time();
        
        //mqtt服务器初始化
        mqtt_client_init();

        //获取最新版本固件
        http_get_latest_version(true);

        //如果判断为需要OTA
        if(strlen(get_global_data()->m_newest_firmware_url) != 0 && strcmp(get_global_data()->m_version, FIRMWARE_VERSION) != 0)
        {
            is_need_ota = 0;
            lock_lvgl();
            switch_screen(ui_OTAScreen);
            char version_change[150];
            sprintf(version_change, "V %s--------->V %s", FIRMWARE_VERSION, get_global_data()->m_version);
            set_text_without_change_font(ui_VersionChange, version_change);
            button_choose_ota_left = true;
            lv_obj_add_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
            lv_obj_clear_state(ui_OTAButtonStart, LV_STATE_PRESSED );
            release_lvgl();
            ota_Wait_tick = 300;      
            ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_ota_button_state);
            ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_ota_button_state);
            ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(Confirm_ota_button_state);
            while(is_need_ota == 0)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                ota_Wait_tick--;  
                if(ota_Wait_tick <= 0)
                {
                    is_need_ota = 2;
                }
            }
        }
        else
        {
            ESP_LOGI("OTA", "No need OTA, newest version");
        }
        //如果需要OTA则不进行初始化
        if(is_need_ota == 1) return;

        //刷新一下focus状态
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;
        http_get_todo_list(true);
        is_init = true;

        //初始化EspNowHost
        EspNowHost::Instance()->init();
    }
    //从机的初始化流程
    else if(get_global_data()->m_is_host == 2)
    {
        //初始化EspNowSlave
        EspNowSlave::Instance()->init(get_global_data()->m_host_mac, get_global_data()->m_host_channel, get_global_data()->m_userName);

        while(EspNowSlave::Instance()->is_host_connected == false)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //绑定主机
        EspNowSlave::Instance()->slave_send_espnow_http_bind_host_request();

        //获取任务列表
        EspNowSlave::Instance()->slave_send_espnow_http_get_todo_list();

        is_init = true;
    }
}

void InitState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_ota_button_state);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_ota_button_state);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(Confirm_ota_button_state);
    elabelUpdateTick = 0;
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}