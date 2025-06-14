#include "InfoState.hpp"
#include "control_driver.hpp"
#include "esp_now_client.hpp"
void confirm_ota_button()
{
    ESP_LOGI("InfoState", "confirm_ota_button");
    if(InfoState::Instance()->m_info_type == INFO_TYPE_OTA)
    {
        if(get_global_data()->m_is_host == 2)
        {
            EspNowClient::Instance()->stop_test_connecting_task(false);
        }
        InfoState::Instance()->need_forward_ota = true;
    }
    else if(InfoState::Instance()->m_info_type == INFO_TYPE_REBOOT)
    {
        if(get_global_data()->m_is_host == 2)
        {
            EspNowClient::Instance()->stop_test_connecting_task(false);
        }
        esp_restart();
    }
}

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
    InfoState::Instance()->need_back_choose_task = true;
}

void InfoState::Init(ElabelController* pOwner)
{
    m_info_type = INFO_TYPE_DEVICE;
    need_flash_paper = false;
    need_back_choose_task = false;
    need_forward_ota = false;
}

void InfoState::Enter(ElabelController* pOwner)
{
    m_info_type = INFO_TYPE_DEVICE;
    need_flash_paper = false;
    need_back_choose_task = false;
    need_forward_ota = false;

    lock_lvgl();
    switch_screen(ui_MessageScreen);
    flush_device_info();
    update_bar();
    release_lvgl();

    if(get_global_data()->m_is_host == 2)
    {
        EspNowClient::Instance()->start_test_connecting_task(false);
    }

    ControlDriver::Instance()->button_press_together_58.Togetherlongpress.registerCallback(out_info);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(previous_info);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(next_info);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_ota_button);
}
void InfoState::Execute(ElabelController* pOwner)
{
    if(need_back_choose_task) return;
    if(need_forward_ota) return;

    if(m_info_type == INFO_TYPE_DEVICE)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            flush_device_info();
            update_bar();
            release_lvgl();
            need_flash_paper = false;
        }
    }
    else if(m_info_type == INFO_TYPE_CONNECTION)
    {
        //如果是从机则更新连接质量，如果连接质量有变化则需要更新页面
        if(get_global_data()->m_is_host == 2)
        {
            if(connect_strength != EspNowClient::Instance()->test_connecting_send_count && EspNowClient::Instance()->test_connecting_send_count != -1)
            {
                need_flash_paper = true;
                connect_strength = EspNowClient::Instance()->test_connecting_send_count;
            }
        }

        //更新开机时间
        char now_live_time[30];
        get_open_time(now_live_time);
        if(strcmp(now_live_time, Live_time) != 0)
        {
            need_flash_paper = true;
            memcpy(Live_time, now_live_time, 30);
        }
        
        if(m_info_type != INFO_TYPE_CONNECTION) return;
        if(need_flash_paper)
        {
            lock_lvgl();
            flush_connection_info();
            update_bar();
            release_lvgl();
            //因为这个界面会自主刷新，所以需要加上判断来保证不和按键刷新冲突
            if(m_info_type == INFO_TYPE_CONNECTION) need_flash_paper = false;
        }
    }
    else
    {
        if(m_info_type == INFO_TYPE_OTA)
        {
            if(need_flash_paper)
            {
                lock_lvgl();
                flush_power_info_ota();
                update_bar();
                release_lvgl();
                need_flash_paper = false;
            }
        }
        else if(m_info_type == INFO_TYPE_REBOOT)
        {
            if(need_flash_paper)
            {
                lock_lvgl();
                flush_power_info_reboot();
                update_bar();
                release_lvgl();
                need_flash_paper = false;
            }
        }
    }
}

void InfoState::Exit(ElabelController* pOwner)
{
    EspNowClient::Instance()->stop_test_connecting_task(false);
    ControlDriver::Instance()->button_press_together_58.Togetherlongpress.unregisterCallback(out_info);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(previous_info);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(next_info);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_ota_button);
}

