#include "SlaveActiveState.hpp"
#include "Esp_now_client.hpp"
#include "network.h"
#include "control_driver.hpp"
#include "esp_random.h"
#include "global_message.h"

void change_slave_active_button_choice()
{
    if(SlaveActiveState::Instance()->slave_active_process == Slaveactive_waiting_connect_process)
    {
        SlaveActiveState::Instance()->button_slave_active_confirm_left = !SlaveActiveState::Instance()->button_slave_active_confirm_left;
        SlaveActiveState::Instance()->need_flash_paper = true;
    }
}

void confirm_slave_active_button_choice()
{
    ESP_LOGI("SlaveActiveState", "confirm_slave_active_button_choice");
    //如果是在测试连接状态
    if(SlaveActiveState::Instance()->slave_active_process == Slaveactive_test_connect_process)
    {
        SlaveActiveState::Instance()->slave_active_process = Slaveactive_bind_host_process;
    }
    //如果是在确定激活者的状态
    else if(SlaveActiveState::Instance()->slave_active_process == Slaveactive_waiting_connect_process)
    { 
        if(SlaveActiveState::Instance()->button_slave_active_confirm_left)
        {
            SlaveActiveState::Instance()->need_back = true;
        }
        else
        {
            SlaveActiveState::Instance()->enter_test_connect();
        }
    }
}

void SlaveActiveState::Init(ElabelController* pOwner)
{
}

void SlaveActiveState::Enter(ElabelController* pOwner)
{
    //停止蓝牙激活
    stop_blue_activate();
    //停止寻找信道
    EspNowClient::Instance()->stop_find_channel();
    //稳妥起见,再设置一次wifi_channel，怕停止信道后被操作了
    uint8_t actual_wifi_channel = 0;
    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
    esp_wifi_set_channel(get_global_data()->m_host_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_get_channel(&actual_wifi_channel, &wifi_second_channel);
    ESP_LOGI(ESP_NOW, "Get Host, set espnow channel to %d", actual_wifi_channel);
    
    button_slave_active_confirm_left = false;
    need_back = false;
    need_flash_paper = false;
    enter_connect_host();
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ESP_LOGI(STATEMACHINE,"Enter SlaveActiveState.");
}

void SlaveActiveState::Execute(ElabelController* pOwner)
{
    if(slave_active_process == Slaveactive_waiting_connect_process)
    {  
        if(need_flash_paper)
        {
            lock_lvgl();
            //重新刷新按钮
            if(button_slave_active_confirm_left)
            {
                lv_obj_add_state(ui_SlaveActiveCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_SlaveActiveConfirm, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_SlaveActiveCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_SlaveActiveConfirm, LV_STATE_PRESSED );
            }
            Global_data* global_data = get_global_data();
            set_text_without_change_font(ui_Username, global_data->m_userName);
            release_lvgl();
            need_flash_paper = false;
        }
    }
    else if(slave_active_process == Slaveactive_test_connect_process)
    {
        if(score!=EspNowClient::Instance()->test_connecting_send_count && EspNowClient::Instance()->test_connecting_send_count!=-1)
        {
            score = EspNowClient::Instance()->test_connecting_send_count;
            lock_lvgl();
            char buffer[32];
            sprintf(buffer, "score: %d", EspNowClient::Instance()->test_connecting_send_count);
            set_text_without_change_font(ui_ConnectGuide2, buffer);
            release_lvgl();
        }
    }
    else if(slave_active_process == Slaveactive_bind_host_process)
    {
        EspNowClient::Instance()->stop_test_connecting_task();
        espnow_add_peer(get_global_data()->m_host_mac, NULL);
        uint8_t temp_data = 0;
        esp_err_t ret; 
        do{
            ret = EspNowClient::Instance()->send_message(&temp_data, 1, Slave2Host_Bind_Request_Http, get_global_data()->m_host_mac);
        }while(ret!=ESP_OK);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //保存激活信息
        get_global_data()->m_is_host = 2;
        //更新nvs
        set_nvs_info_uint8_t_array("is_host",&get_global_data()->m_is_host,1);
        SlaveActiveState::Instance()->slave_active_process = Slaveactive_success_connect_process;
        lock_lvgl();
        set_text_without_change_font(ui_ConnectGuide2, "Success");
        release_lvgl();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }
}   

void SlaveActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    EspNowClient::Instance()->stop_test_connecting_task();
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}