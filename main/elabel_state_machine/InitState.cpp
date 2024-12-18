#include "InitState.hpp"
#include "network.h"
#include "ui.h"
#include "ssd1680.h"
#include "http.h"
#include "m_mqtt.h"
#include "control_driver.h"
#include "ota.h"

void out_ota()
{
    InitState::Instance()->ota_Wait_tick = 300;
}


void InitState::Init(ElabelController* pOwner)
{
    
}

void InitState::Enter(ElabelController* pOwner)
{
    set_led_state(device_init);
    is_init = false;
    set_bit_map_state(HALFMIND_STATE);
    lock_lvgl();
    lv_scr_load(ui_HalfmindScreen);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter InitState.");
}

void InitState::Execute(ElabelController* pOwner)
{
    if(get_wifi_status()!=2) return;
    if(is_init) return;
    //等待2swifi连接两秒稳定
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    set_led_state(device_init);

    //同步时间(堵塞等待)
    HTTP_syset_time();
    get_unix_time();

    //mqtt服务器初始化
    mqtt_client_init();

    //获取最新版本固件
    http_get_latest_version(true);

    if(get_global_data()->newest_firmware_url!=NULL)
    {
        lv_scr_load(ui_OTAScreen);
        lv_label_set_text(ui_Label3, get_global_data()->version);
        ota_Wait_tick = 0;
        //按压开始ota
        register_button_up_short_press_call_back(start_ota);
        register_button_down_short_press_call_back(start_ota);
        //旋转放弃ota
        register_encoder_left_circle_call_back(out_ota);
        register_encoder_right_circle_call_back(out_ota);    
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
    http_bind_user(true);
    is_init = true;
}

void InitState::Exit(ElabelController* pOwner)
{
    set_led_state(no_light);
    elabelUpdateTick = 0;
    //按压开始ota
    unregister_button_down_short_press_call_back(start_ota);
    unregister_button_up_short_press_call_back(start_ota);
    //旋转放弃ota
    unregister_encoder_left_circle_call_back(out_ota);
    unregister_encoder_right_circle_call_back(out_ota);  
    ESP_LOGI(STATEMACHINE,"Out InitState.\n");
}