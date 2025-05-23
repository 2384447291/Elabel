#include "OTAState.hpp"
#include "ota.h"

void OTAState::Init(ElabelController* pOwner)
{
    
}

void OTAState::Enter(ElabelController* pOwner)
{
    lock_lvgl();
    set_text_without_change_font(ui_VersionnNmber, get_global_data()->m_version);
    switch_screen(ui_UpdateScreen);
    lv_bar_set_value(ui_Bar, 0, LV_ANIM_OFF);
    release_lvgl();
    start_ota();
    ESP_LOGI(STATEMACHINE,"Enter OTAScreen.");
}

void OTAState::Execute(ElabelController* pOwner)
{
    if(get_ota_status() == ota_success)
    {
        ESP_LOGI("OTA", "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        lock_lvgl();
        lv_bar_set_value(ui_Bar, 100, LV_ANIM_OFF);
        set_text_without_change_font(ui_Updating,"OTA Success,Restart...");
        release_lvgl();        
        //等2s把字刷出来
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if(get_ota_status() == ota_fail)
    {
        lock_lvgl();
        lv_bar_set_value(ui_Bar, 0, LV_ANIM_OFF);
        set_text_without_change_font(ui_Updating,"OTA Fail,Restart...");
        release_lvgl();
        //等2s把字刷出来
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if(get_ota_status() == ota_ing)
    {
        if(elabelUpdateTick % 2000 == 0)
        {
            lock_lvgl();
            lv_bar_set_value(ui_Bar, (int)get_ota_progress(), LV_ANIM_OFF);
            release_lvgl();
        }
    }
}

void OTAState::Exit(ElabelController* pOwner)
{   
}