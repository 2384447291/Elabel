#include "EasyInfoState.hpp"
#include "control_driver.hpp"
void out_easy_info()
{
    esp_restart();
}

void EasyInfoState::Init(ElabelController* pOwner)
{
}

void EasyInfoState::Enter(ElabelController* pOwner)
{
    lock_lvgl();
    switch_screen(ui_MessageScreen);
    flush_device_info();
    release_lvgl();

    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(out_easy_info);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(out_easy_info);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(out_easy_info);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(out_easy_info);
}
void EasyInfoState::Execute(ElabelController* pOwner)
{
}

void EasyInfoState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(out_easy_info);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(out_easy_info);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(out_easy_info);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(out_easy_info);
}

