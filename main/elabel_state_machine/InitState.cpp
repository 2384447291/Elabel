#include "InitState.hpp"
#include "http.h"
#include "m_mqtt.h"
#include "control_driver.hpp"
#include "network.h"
#include "Esp_now_client.hpp"
#include "Esp_now_slave.hpp"
#include "Esp_now_host.hpp"
#include "SleepState.hpp"

static bool check_firmware_once = false;


void InitState::Init(ElabelController* pOwner)
{
    
}

void InitState::Enter(ElabelController* pOwner)
{
    is_init = false;
    need_enter_ota = false;
    //休眠状态的返回不刷新界面
    if(pOwner->m_elabelFsm.GetPreviousState() != SleepState::Instance())
    {
        lock_lvgl();
        switch_screen(ui_HalfmindScreen);
        release_lvgl();
    }

    ESP_LOGI(STATEMACHINE,"Enter InitState.");
    if(get_global_data()->m_is_host == 2)
    {
        EspNowSlave::Instance()->waiting_host_feedback = false;
    }
}

void InitState::Execute(ElabelController* pOwner)
{
    //主机的初始化流程
    if(get_global_data()->m_is_host == 1)
    {
        //如果用户没有激活，则不进行初始化
        if(strlen(get_global_data()->m_usertoken) == 0)
        {
            //如果用户没有激活，则不进行初始化
            return;
        }

        //如果wifi没有连接，则不进行初始化
        if(get_wifi_status() != 2)
        {
            return;
        }

        //如果已经初始化或者需要OTA则不进行初始化
        if(is_init) 
        {
            ESP_LOGI(STATEMACHINE, "Already initialized or need OTA, not init");
            return;
        }

        if(need_enter_ota) return;

        //如果上一个任务的来源是otaprepare说明ota被拒绝了所以跳过这个判断
        if(!check_firmware_once)
        {
            check_firmware_once = true;
            //获取最新版本固件
            bool get_firmware_need_update = http_get_latest_version(true);

            //如果判断为需要OTA
            if(get_firmware_need_update && strlen(get_global_data()->m_newest_firmware_url) != 0 && strcmp(get_global_data()->m_version, FIRMWARE_VERSION) != 0)
            {
                need_enter_ota = true;
            }
            else
            {
                ESP_LOGI("OTA", "No need OTA, newest version");
            }
        }

        if(need_enter_ota) return;

        //刷新一下focus状态
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        //时间同步(堵塞等待)
        HTTP_syset_time();
        //获取挂墙时间
        get_unix_time();

        //获取任务列表  
        http_get_todo_list(true);

        //获取设备设置项
        http_find_device(true);

        //mqtt服务器初始化
        mqtt_client_init();

        //初始化EspNowHost
        EspNowHost::Instance()->init();

        is_init = true;
    }
    //从机的初始化流程
    else if(get_global_data()->m_is_host == 2)
    {
        //等待收到一帧反馈
        if(!EspNowSlave::Instance()->waiting_host_feedback)
        {
            return;
        }

        if(need_enter_ota) return;

        if(!check_firmware_once)
        {
            check_firmware_once = true;

            //获取wifi
            esp_err_t ret = EspNowSlave::Instance()->slave_send_espnow_http_get_wifi_info();
            //等待2s收到反馈
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            //如果判断为需要OTA
            if(ret == ESP_OK && strcmp(get_global_data()->m_version, FIRMWARE_VERSION) != 0)
            {
                need_enter_ota = true;
            }
            else
            {
                ESP_LOGI("OTA", "No need OTA, newest version");
            }
        }

        if(need_enter_ota) return;

        //刷新一下focus状态
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        //激活
        EspNowSlave::Instance()->slave_send_espnow_http_wakeup_request();
        
        //获取任务列表
        EspNowSlave::Instance()->slave_send_espnow_http_get_todo_list();

        //获取设备设置项
        EspNowSlave::Instance()->slave_send_espnow_http_get_device_info();

        //获取挂墙时间
        EspNowSlave::Instance()->slave_send_espnow_http_get_time();

        is_init = true;
    }
}

void InitState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}