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
    if(get_wifi_status() == 1 && get_global_data()->usertoken!=NULL) return;
    if(is_init) return;
    ControlDriver::Instance()->getLED().setLedState(LedState::DEVICE_INIT);
    //等待1swifi连接两秒稳定
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    //获取挂墙时间(堵塞等待)
    HTTP_syset_time();
    get_unix_time();
    
    //mqtt服务器初始化
    mqtt_client_init();

    //获取最新版本固件
    http_get_latest_version(true);

    //如果判断为需要OTA
    if(get_global_data()->newest_firmware_url != NULL && strcmp(get_global_data()->version, FIRMWARE_VERSION) != 0)
    {
        lv_scr_load(ui_OTAScreen);
        char version_change[100];
        sprintf(version_change, "V %s--------->V %s", FIRMWARE_VERSION, get_global_data()->version);
        set_text(ui_VersionChange, version_change);
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
    ControlDriver::Instance()->getLED().setLedState(LedState::NO_LIGHT);
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}