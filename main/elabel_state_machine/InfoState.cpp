#include "InfoState.hpp"
#include "control_driver.hpp"

void next_info()
{
    ESP_LOGI("InfoState", "next_info");
    uint8_t info_type = InfoState::Instance()->m_info_type;
    info_type++;
    if(info_type >= INFO_TYPE_NUM)
    {
        InfoState::Instance()->m_info_type = INFO_TYPE_DEVICE;
    }
    else
    {
        InfoState::Instance()->m_info_type = (InfoType)info_type;
    }
    InfoState::Instance()->need_flash_paper = true;
}

void previous_info()
{
    ESP_LOGI("InfoState", "previous_info");
    uint8_t info_type = InfoState::Instance()->m_info_type;
    info_type--;
    if(info_type < INFO_TYPE_DEVICE)
    {
        InfoState::Instance()->m_info_type = (InfoType)(INFO_TYPE_NUM - 1);
    }
    else
    {
        InfoState::Instance()->m_info_type = (InfoType)info_type;
    }
    InfoState::Instance()->need_flash_paper = true;
}

void out_info()
{
    InfoState::Instance()->need_out_state = true;
}

void InfoState::Init(ElabelController* pOwner)
{
    m_info_type = INFO_TYPE_DEVICE;
    need_flash_paper = false;
    need_out_state = false;
}

void InfoState::Enter(ElabelController* pOwner)
{
    m_info_type = INFO_TYPE_DEVICE;
    need_flash_paper = false;
    need_out_state = false;
    lock_lvgl();
    switch_screen(ui_MessageScreen);
    flush_device_info();
    update_bar();
    release_lvgl();
    ControlDriver::Instance()->button_press_together_58.Togetherlongpress.registerCallback(out_info);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(previous_info);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(next_info);
}
void InfoState::Execute(ElabelController* pOwner)
{
    if(need_out_state) return;
    if(need_flash_paper)
    {
        if(m_info_type == INFO_TYPE_DEVICE)
        {
            lock_lvgl();
            flush_device_info();
            update_bar();
            release_lvgl();
        }
        else if(m_info_type == INFO_TYPE_CONNECTION)
        {
            lock_lvgl();
            flush_connection_info();
            update_bar();
            release_lvgl();
        }
        need_flash_paper = false;
    }
}

void InfoState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button_press_together_58.Togetherlongpress.unregisterCallback(out_info);
    ControlDriver::Instance()->button6.CallbackLongPress.unregisterCallback(previous_info);
    ControlDriver::Instance()->button7.CallbackLongPress.unregisterCallback(next_info);
}



