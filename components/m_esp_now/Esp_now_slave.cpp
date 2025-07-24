#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "global_nvs.h"
#include "codec.hpp"
#include "global_time.h"
#include "ElabelController.hpp"
static esp_err_t Slave_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;

    //必须要和自己绑定的主机一样的信号才可以算数
    if(Same_mac(get_global_data()->m_host_mac, (uint8_t *)(src_addr)))
    {
        EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
        EspNowSlave::Instance()->waiting_host_feedback = true;
    }

    if (m_message_type == Host2Slave_Bind_Control_Http)
    {
        // 一定要收到原来主机的消息
        if (Same_mac(get_global_data()->m_host_mac, (uint8_t *)(src_addr)) && !EspNowClient::Instance()->is_connect_to_host)
        {
            ESP_LOGI(ESP_NOW, "Receive Bind_Control_Host2Slave message.");
            // 更新连接主机的信息
            Global_data *global_data = get_global_data();
            global_data->m_host_channel = data_ptr[0];

            memcpy(global_data->m_userName, &data_ptr[1], size - 1);
            get_global_data()->m_userName[size - 1] = '\0';

            memcpy(global_data->m_host_mac, (uint8_t *)(src_addr), ESP_NOW_ETH_ALEN);
            ESP_LOGI(ESP_NOW, "Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d",
                     global_data->m_userName,
                     MAC2STR(global_data->m_host_mac),
                     global_data->m_host_channel);
            // 更新nvs
            set_nvs_info_set_host_message(global_data->m_host_mac, global_data->m_host_channel, global_data->m_userName);
            // 保证nvs设置完毕
            vTaskDelay(pdMS_TO_TICKS(1000));
            EspNowClient::Instance()->is_connect_to_host = true;
        }
    }
    else if(m_message_type == Host2Slave_Unbind_Device_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Unbind_Device_Control_Mqtt message from " MACSTR, MAC2STR(src_addr));
        if(Same_mac(get_global_data()->m_host_mac, (uint8_t *)(src_addr)))
        {
            ESP_LOGI(ESP_NOW, "Host " MACSTR " unbind device", MAC2STR(src_addr));
            vTaskDelay(pdMS_TO_TICKS(1000));
            reset_elabel();
        }
    }
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//
    else if (m_message_type == Test_Feedback_Host2Slave)
    {
        ESP_LOGI(ESP_NOW, "Receive Test_Feedback_Host2Slave message.");
        EspNowClient::Instance()->test_connecting_send_count = data_ptr[0] | (data_ptr[1] << 8);
        ESP_LOGI(ESP_NOW, "Test Connecting Send Count: %d", EspNowClient::Instance()->test_connecting_send_count);
    }
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//
    else if(m_message_type == Host2Slave_UpdateTaskList_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateTaskList_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_todo_list(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Send_Task_List_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Send_Task_List_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_send_task_list(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Device_Info_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Device_Info_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_device_info(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Get_Time_Control_Mqtt)
    {
        if(EspNowSlave::Instance()->sleep_sync_flag == 0)
        {
            EspNowSlave::Instance()->sleep_sync_flag = 2;
            printf("get sleep sync response Time sync ");
        }
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_Time_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_time(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Enter_Focus_Control_Mqtt)
    {
        if(EspNowSlave::Instance()->sleep_sync_flag == 0)
        {
            EspNowSlave::Instance()->sleep_sync_flag = 1;
            focus_message_t focus_message = data_to_focus_message(data_ptr);
            printf("get sleep sync response Enter Focus task %d", focus_message.focus_id);
            if(get_global_data()->focusing_task_id == focus_message.focus_id)
            {
                return ESP_OK;
            }
        }
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_enter_focus(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Out_Focus_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Out_Focus_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_out_focus();
    }
    else if(m_message_type == Host2Slave_Get_Wifi_Info_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_Wifi_Info_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_wifi_info(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Get_UserToken_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_UserToken_Control_Mqtt message.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_user_token(data_ptr, size);
    }
    return ESP_OK;
}

void EspNowSlave::init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100])
{
    //更新EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
    EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
    if(EspNowClient::Instance()->m_role == slave_role)
    {
        ESP_LOGE(ESP_NOW, "EspNowSlave already init, role is slave");
        return;
    }
    //初始化espnowslave参数
    memcpy(this->host_mac, host_mac, ESP_NOW_ETH_ALEN);
    this->host_channel = host_channel;
    memcpy(this->username, username, sizeof(this->username));

    EspNowClient::Instance()->m_role = slave_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //设置信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    espnow_add_peer(host_mac, NULL);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Slave_handle);
    ESP_LOGI(ESP_NOW, "Slave init success");
}

void EspNowSlave::deinit()
{
    if(EspNowClient::Instance()->m_role == default_role)
    {
        // ESP_LOGE(ESP_NOW, "EspNowSlave deinit failed, role is default");
        return;
    }

    EspNowClient::Instance()->m_role = default_role;
    espnow_del_peer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

//每次唤醒需要重新设置这个
void EspNowSlave::resume_espnow()
{
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    espnow_add_peer(host_mac, NULL);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Slave_handle);
}



//----------------------------------------------------------------------------从机请求主机的函数----------------------------------------------------------------------------//
esp_err_t EspNowSlave::slave_send_espnow_http_sleep_request()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Sleep_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send sleep request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send sleep request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_wakeup_request()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Wakeup_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send wakeup request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send wakeup request message success");
    }
    return ret;
}

//这个函数会触发主动刷新firmware_need_update
esp_err_t EspNowSlave::slave_send_espnow_http_get_todo_list()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_UpdateTaskList_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send update task list request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send update task list request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_get_device_info()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Get_Device_Info_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send get device info request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send get device info request message success");
    }
    return ret;
}


esp_err_t EspNowSlave::slave_send_espnow_http_synchronous_request()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message_once(&temp_data, 1, Slave2Host_Synchronous_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send synchronous request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send synchronous request message success");
    }
    return ret;
}


esp_err_t EspNowSlave::slave_send_espnow_http_get_time()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Get_Time_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send get time request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send get time request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_enter_focus_task(focus_message_t focus_message)
{
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    esp_err_t ret = send_message(temp_data, temp_data_len, Slave2Host_Enter_Focus_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send enter focus task request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send enter focus task request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_out_focus_task(focus_message_t focus_message)
{
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    esp_err_t ret = send_message(temp_data, temp_data_len, Slave2Host_Out_Focus_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send out focus task request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send out focus task request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_get_wifi_info()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Get_Wifi_Info_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send get wifi info request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send get wifi info request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_get_user_token()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Get_UserToken_Request_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send get user token request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send get user token request message success");
    }
    return ret;
}
esp_err_t EspNowSlave::slave_send_espnow_http_unbind_device()
{
    uint8_t temp_data = 0;
    esp_err_t ret = send_message(&temp_data, 1, Slave2Host_Unbind_Device_Control_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send unbind device request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send unbind device request message success");
    }
    return ret;
}
esp_err_t EspNowSlave::slave_send_espnow_http_send_power_message(int power_state)
{
    uint8_t temp_data[4];
    temp_data[0] = power_state >> 24;
    temp_data[1] = (power_state >> 16) & 0xFF;
    temp_data[2] = (power_state >> 8) & 0xFF;
    temp_data[3] = power_state & 0xFF;
    esp_err_t ret = send_message(temp_data, 4, Slave2Host_Send_power_message_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send unbind device request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send unbind device request message success");
    }
    return ret;
}

esp_err_t EspNowSlave::slave_send_espnow_http_delete_task(int task_id)
{
    uint8_t temp_data[4];
    temp_data[0] = task_id >> 24;
    temp_data[1] = (task_id >> 16) & 0xFF;
    temp_data[2] = (task_id >> 8) & 0xFF;
    temp_data[3] = task_id & 0xFF;
    esp_err_t ret = send_message(temp_data, 4, Slave2Host_Delete_Task_Http);
    if(ret != ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Slave send delete task request message failed");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Slave send delete task request message success");
    }
    return ret;
}

//----------------------------------------------------------------------------从机请求主机的函数----------------------------------------------------------------------------//


//----------------------------------------------------------------------------从机回复主机的函数----------------------------------------------------------------------------//
void EspNowSlave::slave_respense_espnow_mqtt_send_task_list(uint8_t* data, size_t size)
{
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    bool clear_flag = data[1];  // 获取清除标志

    // 如果是第一个包且需要清除之前的数据
    if(clear_flag) {
        clean_todo_list(get_global_data()->m_todo_list);
    }

    // 从数据包中解析任务，注意偏移量需要加上MAC地址的长度 
    size_t offset = 2; // MAC地址(6字节) + 头部(2字节)
    while(offset < size) 
    {
        // 读取任务长度
        size_t title_len = data[offset++];
        
        // 读取任务ID（4字节）
        uint32_t task_id = (data[offset] << 24) | 
                          (data[offset + 1] << 16) |
                          (data[offset + 2] << 8) |
                          data[offset + 3];
        offset += 4;

        // 读取任务内容
        char task_title[50];
        memcpy(task_title, &data[offset], title_len);
        task_title[title_len] = '\0';
        offset += title_len;

        TodoItem todo;
        cleantodoItem(&todo);
        todo.id = task_id;
        todo.title = task_title;
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
    }
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//

    set_task_list_state(firmware_need_update);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_device_info(uint8_t* data, size_t size)
{
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Device_Info_Control_Mqtt message, data: %s", data);
    get_global_data()->m_device_info.default_counter_time = (data[0] << 8) | data[1];
    get_global_data()->m_device_info.overtime_alert_time = (data[2] << 8) | data[3];
    get_global_data()->m_device_info.is_idel_clock_time = data[4] & 0x01;
    get_global_data()->m_device_info.sound_volume = data[5];
    get_global_data()->m_device_info.sleep_time = (data[6] << 8) | data[7];
    get_global_data()->m_device_info.is_strong_wake_up = data[8] & 0x01;
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Device_Info_Control_Mqtt message, default counter time: %d, overtime alert time: %d, is idle clock time: %d, sound volume: %d, sleep time: %d, is strong wake up: %d", 
    get_global_data()->m_device_info.default_counter_time, 
    get_global_data()->m_device_info.overtime_alert_time, 
    get_global_data()->m_device_info.is_idel_clock_time, 
    get_global_data()->m_device_info.sound_volume,
    get_global_data()->m_device_info.sleep_time,
    get_global_data()->m_device_info.is_strong_wake_up);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_time(uint8_t* data, size_t size)
{
    long long nowTime = ((int64_t)data[0] << 56) |
                  ((int64_t)data[1] << 48) |
                  ((int64_t)data[2] << 40) |
                  ((int64_t)data[3] << 32) |
                  ((int64_t)data[4] << 24) |
                  ((int64_t)data[5] << 16) |
                  ((int64_t)data[6] << 8)  |
                  ((int64_t)data[7]);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_Time_Control_Mqtt message, time: %lld", nowTime);
    EspNow_syset_time(nowTime);
    Log_time();
}

void EspNowSlave::slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size)
{
    slave_send_espnow_http_get_todo_list();
}

void EspNowSlave::slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size)
{
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    focus_message_t focus_message = data_to_focus_message(data);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message, focus type: %d, focus time: %d, focus id: %d", focus_message.focus_type, focus_message.fallTiming, focus_message.focus_id);

    TodoItem todo;
    cleantodoItem(&todo);

    todo.taskType = focus_message.focus_type;
    todo.fallTiming = focus_message.fallTiming;
    todo.startTime = focus_message.enter_focus_time;
    todo.id = focus_message.focus_id;
    todo.isFocus = 1;
    todo.title = focus_message.task_name;

    clean_todo_list(get_global_data()->m_todo_list);
    add_or_update_todo_item(get_global_data()->m_todo_list, todo);
    set_task_list_state(firmware_need_update);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_out_focus()
{
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Out_Focus_Control_Mqtt message");
    clean_todo_list(get_global_data()->m_todo_list);
    //重新拉一下http_todo_list，也会有个firmware_need_update是为了刷新task
    slave_send_espnow_http_get_todo_list();
}

void EspNowSlave::slave_respense_espnow_mqtt_get_wifi_info(uint8_t* data, size_t size)
{

    uint8_t wifi_name_len = data[0];
    uint8_t wifi_password_len = data[1 + wifi_name_len];
    uint8_t firmware_version_len = data[2 + wifi_name_len + wifi_password_len];

    memset(get_global_data()->m_wifi_ssid, 0, sizeof(get_global_data()->m_wifi_ssid));
    memset(get_global_data()->m_wifi_password, 0, sizeof(get_global_data()->m_wifi_password));
    memset(get_global_data()->m_version, 0, sizeof(get_global_data()->m_version));

    memcpy(get_global_data()->m_wifi_ssid, data + 1, wifi_name_len);
    memcpy(get_global_data()->m_wifi_password, data + 1 + wifi_name_len + 1, wifi_password_len);
    memcpy(get_global_data()->m_version, data + 1 + wifi_name_len + 1 + wifi_password_len + 1, firmware_version_len);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_Wifi_Info_Control_Mqtt message, wifi name: %s, wifi password: %s, firmware version: %s", get_global_data()->m_wifi_ssid, get_global_data()->m_wifi_password, get_global_data()->m_version);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_user_token(uint8_t* data, size_t size)
{
    uint8_t user_token_len = data[0];
    memset(get_global_data()->m_usertoken, 0, sizeof(get_global_data()->m_usertoken));
    memcpy(get_global_data()->m_usertoken, data + 1, user_token_len);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Get_UserToken_Control_Mqtt message, user token: %s", get_global_data()->m_usertoken);
}
//----------------------------------------------------------------------------从机回复主机的函数----------------------------------------------------------------------------//