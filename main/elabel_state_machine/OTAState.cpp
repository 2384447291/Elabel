#include "OTAState.hpp"
#include "ota.h"

void OTAState::Init(ElabelController* pOwner)
{
    
}

void OTAState::Enter(ElabelController* pOwner)
{
    is_ota_fail = false;
    process = 0;
    lock_lvgl();
    set_text_without_change_font(ui_VersionnNmber, get_global_data()->m_version);
    switch_screen(ui_UpdateScreen);
    lv_bar_set_value(ui_Bar, process, LV_ANIM_OFF);
    release_lvgl();
    start_ota();
    ESP_LOGI(STATEMACHINE,"Enter OTAScreen.");
}

void OTAState::Execute(ElabelController* pOwner)
{
    if(get_ota_status() == ota_success)
    {
        lock_lvgl();
        lv_bar_set_value(ui_Bar, 100, LV_ANIM_OFF);
        if(get_global_data()->m_language == English)
        {
            set_text_with_change_font(ui_Updating,"OTA Success,Restart...", false);
        }
        else if(get_global_data()->m_language == Chinese)
        {
            set_text_with_change_font(ui_Updating,"远程更新成功，重启中...", false);
        }
        release_lvgl();        
        //等2s把字刷出来
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if(get_ota_status() == ota_fail)
    {
        lock_lvgl();
        lv_bar_set_value(ui_Bar, 0, LV_ANIM_OFF);
        if(get_global_data()->m_language == English)
        {
            set_text_with_change_font(ui_Updating,"OTA Fail,Restart...", false);
        }
        else if(get_global_data()->m_language == Chinese)
        {
            set_text_with_change_font(ui_Updating,"远程更新失败，重启中...", false);
        }
        release_lvgl();
        //等2s把字刷出来
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        is_ota_fail = true;
    }
    else if(get_ota_status() == ota_ing)
    {
        //每2s加1
        if(elabelUpdateTick % 2000 == 0)
        {
            process+=5;
            lock_lvgl();
            lv_bar_set_value(ui_Bar, process, LV_ANIM_OFF);
            release_lvgl();
        }
    }
}

void OTAState::Exit(ElabelController* pOwner)
{   
}