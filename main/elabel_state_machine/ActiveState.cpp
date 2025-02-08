#include "ActiveState.hpp"
#include "network.h"
#include "http.h"

void change_language()
{   
    get_global_data()->m_language = (get_global_data()->language + 1) % 2;
    if(get_global_data()->m_language == 0)
    {
        set_text_english(ui_ActiveGuide, "Use APP to active Host\n"
                                          "or\n"
                                          "Use Host to active Slave");
        set_text_english(ui_HostName, "Host");
        set_text_english(ui_SlaveName, "Slave");
        set_text_english(ui_HostInf, "Username:\n"
                                      "WIFIname:\n"
                                      "WIFIPassword:\n"
                                      "SlaveNum:");
        set_text_english(ui_SlaveInf, "Username:\n"
                                       "HostMac:\n");
        set_text_english(ui_NewFirmware, "New Firmware");
        set_text_english(ui_OperateGuide ,"Click: Apply\n"
                                           "Rotate: Skip");
        set_text_english(ui_Updating, "Updating...");
        set_text_english(ui_FocusTask, "Focus Task");
        set_text_english(ui_ShutdownGuide, "See you next time");
    }
    else if(get_global_data()->m_language == 1)
    {
        set_text_chinese(ui_ActiveGuide, "使用APP激活主机\n"
                                          "或\n"
                                          "使用主机激活从机");
        set_text_chinese(ui_HostName, "主机");
        set_text_chinese(ui_SlaveName, "从机");
        set_text_chinese(ui_HostInf, "用户名:\n"
                                      "WiFi名称:\n"
                                      "WiFi密码:\n"
                                      "从机数量:");
        set_text_chinese(ui_SlaveInf, "用户名:\n"
                                       "主机MAC:");
        set_text_chinese(ui_NewFirmware, "新固件");
        set_text_chinese(ui_OperateGuide, "点击: 应用\n"
                                           "旋转: 跳过");
        set_text_chinese(ui_Updating, "正在更新...");
        set_text_chinese(ui_FocusTask, "专注任务");
        set_text_chinese(ui_ShutdownGuide, "明天再见");
    }
}

void ActiveState::Init(ElabelController* pOwner)
{
    
}

void ActiveState::Enter(ElabelController* pOwner)
{
    ControlDriver::Instance()->getLED().setLedState(LedState::DEVICE_NO_WIFI);
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
    if()
    // if(get_wifi_status()==0x02)
    // {
    //     //等待500ms连接稳定和token接收
    //     vTaskDelay(500 / portTICK_PERIOD_MS);
    //     http_find_usr(true);
    //     http_bind_user(true);
    //     //设定为已经绑定了设备
    //     nvs_handle wificfg_nvs_handler;
    //     ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );
    //     ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"username",get_global_data()->userName));
    //     ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    //     nvs_close(wificfg_nvs_handler);                     /* 关闭 */ 
    //     lock_lvgl();
    //     repaint_para();
    //     release_lvgl();
    //     set_wifi_status(0x03);
    //     ControlDriver::Instance()->getLED().setLedState(LedState::NO_LIGHT);
    // }
}

void ActiveState::Exit(ElabelController* pOwner)
{
    // stop_blue_activate();
    // //如果状态不是0x00，不会连接的
    // m_wifi_connect();
    ESP_LOGI(STATEMACHINE,"Out ActiveState.\n");
    ControlDriver::Instance()->ButtonDownDoubleclick.unregisterCallback(change_language);
    ControlDriver::Instance()->ButtonUpDoubleclick.unregisterCallback(change_language);
}