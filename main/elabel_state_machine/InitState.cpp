#include "InitState.hpp"
#include "http.h"
#include "m_mqtt.h"
#include "control_driver.hpp"
#include "network.h"

void enter_ota();
void out_ota();

void out_ota()
{
    InitState::Instance()->is_need_ota = false;
    InitState::Instance()->ota_Wait_tick = 300;
    ControlDriver::Instance()->ButtonDownShortPress.unregisterCallback(enter_ota);
    ControlDriver::Instance()->ButtonUpShortPress.unregisterCallback(enter_ota);
}

void enter_ota()
{
    InitState::Instance()->is_need_ota = true;
    InitState::Instance()->ota_Wait_tick = 300;
    //旋转放弃ota
    ControlDriver::Instance()->EncoderRightCircle.unregisterCallback(out_ota);
    ControlDriver::Instance()->EncoderLeftCircle.unregisterCallback(out_ota);
}


void InitState::Init(ElabelController* pOwner)
{
    
}

void InitState::Enter(ElabelController* pOwner)
{
    is_init = false;
    is_need_ota = false;
    lock_lvgl();
    lv_scr_load(ui_HalfmindScreen);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter InitState.");
}

void InitState::Execute(ElabelController* pOwner)
{
    //如果正在连接或者没有用户，则不进行初始化
    if(get_wifi_status() == 1 && get_global_data()->m_usertoken!=NULL) return;
    if(is_init) return;

    //等待1swifi连接两秒稳定
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    //时间同步
    HTTP_syset_time();
    //获取挂墙时间(堵塞等待)
    get_unix_time();
    
    //mqtt服务器初始化
    mqtt_client_init();

    //获取最新版本固件
    http_get_latest_version(true);

    //如果判断为需要OTA
    if(strlen(get_global_data()->m_newest_firmware_url) != 0 && strcmp(get_global_data()->m_version, FIRMWARE_VERSION) != 0)
    {
        lock_lvgl();
        lv_scr_load(ui_OTAScreen);
        char version_change[150];
        sprintf(version_change, "V %s--------->V %s", FIRMWARE_VERSION, get_global_data()->m_version);
        set_text_without_change_font(ui_VersionChange, version_change);
        release_lvgl();
        ota_Wait_tick = 0;      
        //短按进入ota
        ControlDriver::Instance()->ButtonDownShortPress.registerCallback(enter_ota);
        ControlDriver::Instance()->ButtonUpShortPress.registerCallback(enter_ota);
        //旋转退出ota
        ControlDriver::Instance()->EncoderRightCircle.registerCallback(out_ota);
        ControlDriver::Instance()->EncoderLeftCircle.registerCallback(out_ota);

        while(true)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ota_Wait_tick++;      
            if(ota_Wait_tick >= 300)
            {
                break;
            }
        }
    }
    else
    {
        ESP_LOGI("OTA", "No need OTA, newest version");
    }
    //刷新一下focus状态
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;
    http_get_todo_list(true);
    is_init = true;
}

void InitState::Exit(ElabelController* pOwner)
{
    elabelUpdateTick = 0;
    ControlDriver::Instance()->ButtonDownShortPress.unregisterCallback(enter_ota);
    ControlDriver::Instance()->ButtonUpShortPress.unregisterCallback(enter_ota);
    ControlDriver::Instance()->EncoderRightCircle.unregisterCallback(out_ota);
    ControlDriver::Instance()->EncoderLeftCircle.unregisterCallback(out_ota);
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}