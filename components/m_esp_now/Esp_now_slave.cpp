#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "global_nvs.h"
#include "codec.hpp"
#include "VoicePacketManager.hpp"
static esp_err_t Slave_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    // ESP_PARAM_CHECK(src_addr);
    // ESP_PARAM_CHECK(data);
    // ESP_PARAM_CHECK(size);
    // ESP_PARAM_CHECK(rx_ctrl);

    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;
    EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
    if(m_message_type == Bind_Control_Host2Slave)
    {
        if(!EspNowSlave::Instance()->is_host_connected)
        {     
            ESP_LOGI(ESP_NOW, "Receive Bind_Control_Host2Slave message.");
            Global_data* global_data = get_global_data();
            global_data->m_host_channel = data_ptr[0];
            memcpy(global_data->m_userName, &data_ptr[1], size-1);
            memcpy(global_data->m_host_mac, (uint8_t*)(src_addr), ESP_NOW_ETH_ALEN);
            ESP_LOGI(ESP_NOW, "Host Reconnect, Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d", 
                global_data->m_userName, 
                MAC2STR(global_data->m_host_mac), 
                global_data->m_host_channel);
            //设置信道
            ESP_ERROR_CHECK(esp_wifi_set_channel(global_data->m_host_channel, WIFI_SECOND_CHAN_NONE));
            //更新nvs
            set_nvs_info_set_host_message(global_data->m_host_mac, global_data->m_host_channel, global_data->m_userName);
            EspNowSlave::Instance()->is_host_connected = true;
        }
    }
    else if(m_message_type == Host2Slave_UpdateTaskList_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateTaskList_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_todo_list(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Enter_Focus_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_enter_focus(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Out_Focus_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Out_Focus_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_out_focus();
    }
    //------------------------------------------------专门用来处理音频收发的接口------------------------------------------------//
    if(m_message_type == Voice_Start_Send)
    {
        ESP_LOGI(ESP_NOW, "Receive Voice_Start_Send from " MACSTR, MAC2STR(src_addr));
        uint32_t recorded_size = (data_ptr[0] << 24) | (data_ptr[1] << 16) | (data_ptr[2] << 8) | data_ptr[3];
        uint16_t packet_num = (data_ptr[4] << 8) | data_ptr[5];
        MCodec::Instance()->recorded_size = 0;
        VoicePacketManager::Instance()->total_packets = packet_num;
        VoicePacketManager::Instance()->voice_size = recorded_size;
    }
    else if(m_message_type == Voice_Stop_Send)
    {
        ESP_LOGI(ESP_NOW, "Receive Voice_Stop_Send from " MACSTR, MAC2STR(src_addr));
        VoicePacketManager::Instance()->send_voice_feedback(src_addr);
    }
    else if(m_message_type == Voice_Message)
    {
        uint16_t voice_packet_id = (data_ptr[0] << 8) | data_ptr[1];
        VoicePacketManager::Instance()->receive_packet(voice_packet_id);
        memcpy(MCodec::Instance()->record_buffer+(voice_packet_id*(MAX_EFFECTIVE_DATA_LEN-2)), data_ptr+2, size-2);
    }
    else if(m_message_type == Voice_Feedback)
    {
        ESP_LOGI(ESP_NOW, "Receive Voice_Feedback from " MACSTR, MAC2STR(src_addr));
        if(VoicePacketManager::Instance()->voice_packet_status == voice_packet_status_feedback)
        {
            uint8_t missing_packets_size = data_ptr[0] << 8 | data_ptr[1];
            if(missing_packets_size == 0)
            {
                VoicePacketManager::Instance()->need_send_mac.removeByMac(src_addr);
            }
            else
            {
                for(int i = 0; i < missing_packets_size; i++)
                {
                    uint16_t missing_packet_id = data_ptr[2+i*2] << 8 | data_ptr[3+i*2];
                    VoicePacketManager::Instance()->receive_packet(missing_packet_id);
                }
            }
            VoicePacketManager::Instance()->voice_packet_status = voice_packet_judge_process;
        }
    }
    //------------------------------------------------专门用来处理音频收发的接口------------------------------------------------//
    return ESP_OK;
}

static void esp_now_send_update(void *pvParameter)
{
    uint8_t channel = 0;
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if(EspNowSlave::Instance()->is_host_connected)
        {
            if(xTaskGetTickCount() - EspNowSlave::Instance()->last_recv_heart_time > pdMS_TO_TICKS(5000))
            {
                EspNowSlave::Instance()->is_host_connected = false;
                ESP_LOGE(ESP_NOW, "Slave not receive heart from host, reconnect");
            }
        }
        else
        {
            channel = channel % 13 + 1;
            uint8_t actual_wifi_channel = 0;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
            ESP_ERROR_CHECK(esp_wifi_get_channel(&actual_wifi_channel, &wifi_second_channel));
            ESP_LOGI(ESP_NOW, "Set espnow channel to %d", actual_wifi_channel);
        }
    }
}

void EspNowSlave::init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100])
{
    if(slave_send_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave already init");
        return;
    }

    //初始化espnowslave参数
    memcpy(this->host_mac, host_mac, ESP_NOW_ETH_ALEN);
    this->host_channel = host_channel;
    memcpy(this->username, username, sizeof(this->username));
    is_host_connected = false;
    last_recv_heart_time = 0;

    EspNowClient::Instance()->m_role = slave_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //设置信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    espnow_add_peer(host_mac, NULL);

    xTaskCreate(esp_now_send_update, "esp_now_slave_send_update_task", 4096, NULL, 0, &slave_send_update_task_handle);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Slave_handle);
    ESP_LOGI(ESP_NOW, "Slave init success");
}

void EspNowSlave::deinit()
{
    if(slave_send_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave not init");
        return;
    }
    EspNowClient::Instance()->m_role = default_role;
    vTaskDelete(slave_send_update_task_handle);
    slave_send_update_task_handle = NULL;

    espnow_del_peer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

void EspNowSlave::slave_send_espnow_http_get_todo_list()
{
    ESP_LOGI(ESP_NOW, "Slave send update task list request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_UpdateTaskList_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_bind_host_request()
{
    ESP_LOGI(ESP_NOW, "Slave send bind host request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_Bind_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_enter_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send enter focus task request message");
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    send_message(temp_data, temp_data_len, Slave2Host_Enter_Focus_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_out_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send out focus task request message");
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    send_message(temp_data, temp_data_len, Slave2Host_Out_Focus_Request_Http);
}


void EspNowSlave::slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size)
{
    EspNowClient::Instance()->print_uint8_array(data, size);
    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;

    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    bool clear_flag = data[ESP_NOW_ETH_ALEN + 1];  // 获取清除标志

    // 如果是第一个包且需要清除之前的数据
    if(clear_flag) {
        clean_todo_list(get_global_data()->m_todo_list);
    }

    // 从数据包中解析任务，注意偏移量需要加上MAC地址的长度 
    size_t offset = ESP_NOW_ETH_ALEN + 2; // MAC地址(6字节) + 头部(2字节)
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

void EspNowSlave::slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size)
{
    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    focus_message_t focus_message = data_to_focus_message(data);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message, focus type: %d, focus time: %d", focus_message.focus_type, focus_message.focus_time);

    TodoItem todo;
    cleantodoItem(&todo);

    todo.taskType = focus_message.focus_type;
    todo.fallTiming = focus_message.focus_time;
    todo.id = focus_message.focus_id;
    todo.isFocus = 1;

    if(focus_message.focus_type == 1)
    {
        char title[20] = "Pure Time Task";;
        todo.title = title;
    }
    else if(focus_message.focus_type == 2 || focus_message.focus_type == 0)
    {
        todo.title = focus_message.task_name;
    }
    else if(focus_message.focus_type == 3)
    {
        char title[20] = "Record Task";
        todo.title = title;
    }
    clean_todo_list(get_global_data()->m_todo_list);
    add_or_update_todo_item(get_global_data()->m_todo_list, todo);
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    set_task_list_state(firmware_need_update);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_out_focus()
{
    //清零列表
    get_global_data()->m_focus_state->is_focus = 2;
    get_global_data()->m_focus_state->focus_task_id = 0;
    clean_todo_list(get_global_data()->m_todo_list);
    set_task_list_state(firmware_need_update);
    //重新拉一下http_todo_list
    slave_send_espnow_http_get_todo_list();
}


