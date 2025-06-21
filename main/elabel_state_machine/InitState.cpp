#include "InitState.hpp"
#include "http.h"
#include "m_mqtt.h"
#include "control_driver.hpp"
#include "network.h"
#include "Esp_now_client.hpp"
#include "Esp_now_slave.hpp"
#include "Esp_now_host.hpp"
#include "SleepState.hpp"
#include "SleepFocusState.hpp"
#include "global_nvs.h"

static bool check_firmware_once = false;

int compare_version_str(const char* v1, const char* v2) {
    // 复制原始，用于日志
    ESP_LOGI("VERSION CHECK", "Compare versions: '%s' vs '%s'", v1, v2);

    // 假设单段长度与总长度不会超过 31 字符
    char buf1[32];
    char buf2[32];
    strncpy(buf1, v1, sizeof(buf1) - 1);
    buf1[sizeof(buf1) - 1] = '\0';
    strncpy(buf2, v2, sizeof(buf2) - 1);
    buf2[sizeof(buf2) - 1] = '\0';

    char *save1 = NULL, *save2 = NULL;
    char *token1 = strtok_r(buf1, ".", &save1);
    char *token2 = strtok_r(buf2, ".", &save2);
    int idx = 0;

    // 只要任一还有段，就继续比较；短的自动当作 0
    while (token1 != NULL || token2 != NULL) {
        int num1 = 0, num2 = 0;
        if (token1) {
            num1 = atoi(token1);
        }
        if (token2) {
            num2 = atoi(token2);
        }
        ESP_LOGI("VERSION CHECK", "Segment %d: '%s'->%d vs '%s'->%d",
                 idx,
                 token1 ? token1 : "(none)", num1,
                 token2 ? token2 : "(none)", num2);

        if (num1 < num2) {
            ESP_LOGI("VERSION CHECK", "Result at segment %d: %d < %d => v1 < v2", idx, num1, num2);
            return -1;
        } else if (num1 > num2) {
            ESP_LOGI("VERSION CHECK", "Result at segment %d: %d > %d => v1 > v2", idx, num1, num2);
            return 1;
        }
        // 相等，继续下一段
        token1 = token1 ? strtok_r(NULL, ".", &save1) : NULL;
        token2 = token2 ? strtok_r(NULL, ".", &save2) : NULL;
        idx++;
    }
    ESP_LOGI("VERSION CHECK", "All segments equal => v1 == v2");
    return 0;
}


void InitState::Init(ElabelController* pOwner)
{
    
}

void InitState::Enter(ElabelController* pOwner)
{
    is_init = false;
    need_enter_ota = false;
    //休眠状态的返回不刷新界面
    if(pOwner->m_elabelFsm.GetPreviousState() != SleepState::Instance() 
    && pOwner->m_elabelFsm.GetPreviousState() != SleepFocusState::Instance())
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

        //保存电源设置
        http_save_power(true,BatteryManager::Instance()->getBatteryLevelInt(),get_global_data()->m_mac_uint);

        //mqtt服务器初始化
        mqtt_client_init();

        //初始化EspNowHost
        EspNowHost::Instance()->init();

        is_init = true;

        get_global_data()->reset_count = 0;
        set_reset_count(get_global_data()->reset_count);
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

            // 复制并准备版本字符串
            char version_str[32] = {0};
            char firmware_str[32] = {0};

            strncpy(version_str, FIRMWARE_VERSION, sizeof(version_str) - 1);
            version_str[sizeof(version_str) - 1] = '\0';
            strncpy(firmware_str, get_global_data()->m_version, sizeof(firmware_str) - 1);
            firmware_str[sizeof(firmware_str) - 1] = '\0';

            int cmp = compare_version_str(version_str, firmware_str);
            
            if(ret == ESP_OK && cmp < 0)
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

        //发送电源状态  
        EspNowSlave::Instance()->slave_send_espnow_http_send_power_message(BatteryManager::Instance()->getBatteryLevelInt());

        is_init = true;

        get_global_data()->reset_count = 0;
        set_reset_count(get_global_data()->reset_count);
    }
}

void InitState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}