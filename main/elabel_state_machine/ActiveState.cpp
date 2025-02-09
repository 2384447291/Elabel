#include "ActiveState.hpp"
#include "network.h"
#include "http.h"

void change_language()
{   
    get_global_data()->m_language = (language)((get_global_data()->m_language + 1) % 2);
    char language_str[10];
    snprintf(language_str, sizeof(language_str), "%d", get_global_data()->m_language);
    set_nvs_info("language",language_str);
    Change_All_language();
}

void ActiveState::Init(ElabelController* pOwner)
{
    
}

void ActiveState::Enter(ElabelController* pOwner)
{
    start_blue_activate();
    lock_lvgl();
    lv_scr_load(ui_ActiveScreen);
    release_lvgl();
    ControlDriver::Instance()->ButtonDownDoubleclick.registerCallback(change_language);
    ControlDriver::Instance()->ButtonUpDoubleclick.registerCallback(change_language);
    ESP_LOGI(STATEMACHINE,"Enter ActiveState.");
}

void ActiveState::Execute(ElabelController* pOwner)
{
}

void ActiveState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ActiveState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(change_language);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(change_language);
}