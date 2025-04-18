#include "SlaveActiveState.hpp"
#include "Esp_now_client.hpp"
#include "network.h"
#include "control_driver.hpp"

TaskHandle_t test_connecting_task_handle = NULL;
bool need_stop_test_connecting = false;
TickType_t start_time = 0;
uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
size_t temp_data_len = MAX_EFFECTIVE_DATA_LEN;
esp_err_t ret;
static void test_connecting_task(void *pvParameter)
{
    for(int i = 0; i < MAX_EFFECTIVE_DATA_LEN; i++)
    {
        temp_data[i] = esp_random() & 0xFF;
    }
    while(!need_stop_test_connecting)
    {
        switch(SlaveActiveState::Instance()->test_connect_process)
        {
            case default_test_connect_process:
            break;
            case test_connect_process_start:
            {
                do{
                    ret = EspNowClient::Instance()->send_message(temp_data, temp_data_len, Test_Start_Request_Slave2Host, get_global_data()->m_host_mac);
                }while(ret!=ESP_OK);
                SlaveActiveState::Instance()->test_connect_process = test_connect_process_send_packet;
            }
            break;
            case test_connect_process_send_packet:
            {
                start_time = xTaskGetTickCount();
                do{
                    ret = EspNowClient::Instance()->send_test_message(temp_data, temp_data_len, get_global_data()->m_host_mac);
                }while(xTaskGetTickCount() - start_time < pdMS_TO_TICKS(2000));
                EspNowClient::Instance()->test_connecting_send_count = 0;
                SlaveActiveState::Instance()->test_connect_process = test_connect_process_stop;
            }
            break;
            case test_connect_process_stop:
            {
                do{
                    ret = EspNowClient::Instance()->send_message(temp_data, temp_data_len, Test_Stop_Request_Slave2Host, get_global_data()->m_host_mac);
                }while(ret!=ESP_OK);
                SlaveActiveState::Instance()->test_connect_process = test_waiting_ack_process;
            }
            break;
            case test_waiting_ack_process:
            {
                while(EspNowClient::Instance()->test_connecting_send_count == 0)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                SlaveActiveState::Instance()->need_flash_paper = true;
                SlaveActiveState::Instance()->test_connect_process = test_connect_process_start;
            }
            break;
        }
    }
    test_connecting_task_handle = NULL;
    vTaskDelete(NULL);
}

void SlaveActiveState::start_test_connecting_task()
{
    if(test_connecting_task_handle == NULL)
    {
        //添加主机为peer
        espnow_add_peer(get_global_data()->m_host_mac, NULL);
        need_stop_test_connecting = false;
        xTaskCreate(test_connecting_task, "test_connecting_task", 4096, NULL, 10, &test_connecting_task_handle);
    }
}

void SlaveActiveState::stop_test_connecting_task()
{
    if(test_connecting_task_handle != NULL)
    {
        //删除主机为peer
        espnow_del_peer(get_global_data()->m_host_mac);
        need_stop_test_connecting = true;
    }
}

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
        SlaveActiveState::Instance()->slave_active_process = Slaveactive_success_connect_process;
        EspNowSlave::Instance()->slave_send_espnow_http_bind_host_request();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //保存激活信息
        get_global_data()->m_is_host = 2;
        //更新nvs
        set_nvs_info_uint8_t_array("is_host",&get_global_data()->m_is_host,1);
        
        SlaveActiveState::Instance()->need_forward = true;
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

    button_slave_active_confirm_left = false;
    need_back = false;
    need_forward = false;
    need_flash_paper = false;
    enter_connect_host();
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_slave_active_button_choice);
    ESP_LOGI(STATEMACHINE,"Enter SlaveActiveState.");
}

void SlaveActiveState::Execute(ElabelController* pOwner)
{
    if(slave_active_process == Slaveactive_test_connect_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            char buffer[32];
            sprintf(buffer, "score: %d", EspNowClient::Instance()->test_connecting_send_count);
            set_text_without_change_font(ui_ConnectGuide2, buffer);
            release_lvgl();
            need_flash_paper = false;
        }
    }
    else if(slave_active_process == Slaveactive_waiting_connect_process)
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
}   

void SlaveActiveState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_slave_active_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_slave_active_button_choice);
    stop_test_connecting_task();
    ESP_LOGI(STATEMACHINE,"Out SlaveActiveState.\n");
}