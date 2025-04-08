#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "global_nvs.h"
#include "codec.hpp"

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(EspNowSlave::Instance()->get_host_status() && xTaskGetTickCount() - EspNowSlave::Instance()->last_recv_heart_time > pdMS_TO_TICKS(5000) && EspNowSlave::Instance()->last_recv_heart_time != 0)
        {
            EspNowSlave::Instance()->set_host_status(false);
            ESP_LOGE(ESP_NOW, "Slave not receive heart from host, reconnect");
        }
        //处理接收到的信息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_packet_queue)<MAX_RECV_PACKET_QUEUE_SIZE)
        {
            espnow_packet_t recv_packet;   
            if(xQueueReceive(EspNowClient::Instance()->recv_packet_queue, &recv_packet, 0) == pdTRUE)
            {
                EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
                //处理接收到的信息
                // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_packet.mac_addr), recv_packet.data_len);
                // EspNowClient::Instance()->print_uint8_array(recv_packet.data, recv_packet.data_len);
                //处理http的请求，如果收到了，则说明主机收到了请求，从机不用再发了，等待主机的mqtt更新即可
                if(recv_packet.m_message_type == Feedback_ACK)
                {
                    uint16_t feedback_unique_id = (recv_packet.data[0] << 8) | recv_packet.data[1];
                    if (EspNowSlave::Instance()->remind_host.unique_id == feedback_unique_id)
                    {
                        if(EspNowSlave::Instance()->need_send_data)
                        {
                            ESP_LOGI(ESP_NOW, "Receive feedback from host, unique id: %d", feedback_unique_id);
                            EspNowSlave::Instance()->finish_current_remind();
                            EspNowSlave::Instance()->need_send_data = false;
                        }
                    }
                }
                else if(recv_packet.m_message_type == Bind_Control_Host2Slave)
                {
                    if(EspNowSlave::Instance()->get_host_status())
                    {
                        continue;
                    }
                    ESP_LOGI(ESP_NOW, "Receive Bind_Control_Host2Slave message unique id: %d", recv_packet.unique_id);
                    Global_data* global_data = get_global_data();
                    global_data->m_host_channel = recv_packet.data[0];
                    memcpy(global_data->m_userName, &recv_packet.data[1], recv_packet.data_len-1);
                    memcpy(global_data->m_host_mac, (uint8_t*)(recv_packet.mac_addr), ESP_NOW_ETH_ALEN);
                    ESP_LOGI(ESP_NOW, "Host Reconnect, Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d", 
                        global_data->m_userName, 
                        MAC2STR(global_data->m_host_mac), 
                        global_data->m_host_channel);
                    //设置信道
                    ESP_ERROR_CHECK(esp_wifi_set_channel(global_data->m_host_channel, WIFI_SECOND_CHAN_NONE));
                    //更新nvs
                    set_nvs_info_set_host_message(global_data->m_host_mac, global_data->m_host_channel, global_data->m_userName);
                    EspNowSlave::Instance()->set_host_status(true);
                }
                else if(recv_packet.m_message_type == Host2Slave_UpdateTaskList_Control_Mqtt)
                {
                    ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateTaskList_Control_Mqtt message unique id: %d", recv_packet.unique_id);
                    EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_todo_list(recv_packet);
                }
                else if(recv_packet.m_message_type == Host2Slave_Enter_Focus_Control_Mqtt)
                {
                    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message unique id: %d", recv_packet.unique_id);
                    EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_enter_focus(recv_packet);
                }
                else if(recv_packet.m_message_type == Host2Slave_Out_Focus_Control_Mqtt)
                {
                    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Out_Focus_Control_Mqtt message unique id: %d", recv_packet.unique_id);
                    EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_out_focus();
                }
                else if(recv_packet.m_message_type == Host2Slave_UpdateRecording_Control_Mqtt)
                {
                    ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateRecording_Control_Mqtt message unique id: %d", recv_packet.unique_id);
                    EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_update_recording(recv_packet);
                }
            }
        }
    }
}

static void esp_now_send_update(void *pvParameter)
{
    uint8_t channel = 0;
    const TickType_t channel_switch_delay = pdMS_TO_TICKS(1000);  // 信道切换间隔1秒 ，这里转tick了所以不用转时间
    TickType_t last_switch_time = xTaskGetTickCount();
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(!EspNowSlave::Instance()->get_host_status())
        {
            // 检查是否需要切换信道
            if ((xTaskGetTickCount() - last_switch_time) >= channel_switch_delay)
            {
                // 只有在未连接到主机时才进行信道切换
                if (!EspNowClient::Instance()->is_connect_to_host)
                {
                    channel = channel % 13 + 1;
                    uint8_t actual_wifi_channel = 0;
                    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
                    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
                    ESP_ERROR_CHECK(esp_wifi_get_channel(&actual_wifi_channel, &wifi_second_channel));
                    ESP_LOGI(ESP_NOW, "Set espnow channel to %d", actual_wifi_channel);
                }
                last_switch_time = xTaskGetTickCount();
            }
        }
        else
        {
            if(EspNowSlave::Instance()->need_send_data)
            {
                //发送remind消息
                EspNowClient::Instance()->send_esp_now_message(EspNowSlave::Instance()->host_mac, 
                    EspNowSlave::Instance()->remind_host.data, 
                    EspNowSlave::Instance()->remind_host.data_len, 
                    EspNowSlave::Instance()->remind_host.m_message_type, 
                    true, 
                    EspNowSlave::Instance()->remind_host.unique_id);
            }
            else
            {
                if(EspNowSlave::Instance()->get_current_remind(&EspNowSlave::Instance()->remind_host))
                {
                    EspNowSlave::Instance()->need_send_data = true;
                }
            }
        }
    }
}

void EspNowSlave::init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100])
{
    memcpy(this->host_mac, host_mac, ESP_NOW_ETH_ALEN);
    this->host_channel = host_channel;
    memcpy(this->username, username, sizeof(this->username));

    if(slave_recieve_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave already init");
        return;
    }
    EspNowClient::Instance()->m_role = slave_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //设置信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    EspNowClient::Instance()->Addpeer(this->host_channel, this->host_mac);

    //初始化host连接状态
    is_host_connected = false;

    //创建remind队列
    remind_queue = xQueueCreate(10, sizeof(remind_host_t));
    if (remind_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Failed to create remind queue!");
        return;
    }

    xTaskCreate(esp_now_recieve_update, "esp_now_slave_recieve_update_task", 4096, NULL, 1, &slave_recieve_update_task_handle);
    xTaskCreate(esp_now_send_update, "esp_now_slave_send_update_task", 4096, NULL, 10, &slave_send_update_task_handle);

    ESP_LOGI(ESP_NOW, "Slave init success");
}

void EspNowSlave::deinit()
{
    if(slave_recieve_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave not init");
        return;
    }
    EspNowClient::Instance()->m_role = default_role;
    vTaskDelete(slave_recieve_update_task_handle);
    vTaskDelete(slave_send_update_task_handle);
    slave_recieve_update_task_handle = NULL;
    slave_send_update_task_handle = NULL;

    // 删除remind队列
    if (remind_queue != NULL) {
        vQueueDelete(remind_queue);
        remind_queue = NULL;
    }
    EspNowClient::Instance()->Delpeer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

void EspNowSlave::slave_send_espnow_http_get_todo_list()
{
    ESP_LOGI(ESP_NOW, "Slave send update task list request message");
    remind_host_t temp_remind_host;
    temp_remind_host.data[0] = 0x00;
    temp_remind_host.data_len = 1;
    temp_remind_host.unique_id = esp_random() & 0xFFFF;
    temp_remind_host.m_message_type = Slave2Host_UpdateTaskList_Request_Http;
    add_remind_to_queue(temp_remind_host);
}

void EspNowSlave::slave_send_espnow_http_bind_host_request()
{
    ESP_LOGI(ESP_NOW, "Slave send bind host request message");
    remind_host_t temp_remind_host;
    temp_remind_host.data[0] = 0x00;
    temp_remind_host.data_len = 1;
    temp_remind_host.unique_id = esp_random() & 0xFFFF;
    temp_remind_host.m_message_type = Slave2Host_Bind_Request_Http;
    add_remind_to_queue(temp_remind_host);
}

void EspNowSlave::slave_send_espnow_http_enter_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send enter focus task request message");
    remind_host_t temp_remind_host;
    focus_message_to_data(focus_message, temp_remind_host.data, temp_remind_host.data_len);
    temp_remind_host.unique_id = esp_random() & 0xFFFF;
    temp_remind_host.m_message_type = Slave2Host_Enter_Focus_Request_Http;
    add_remind_to_queue(temp_remind_host);
}

void EspNowSlave::slave_send_espnow_http_out_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send out focus task request message");
    remind_host_t temp_remind_host;
    focus_message_to_data(focus_message, temp_remind_host.data, temp_remind_host.data_len);
    temp_remind_host.unique_id = esp_random() & 0xFFFF;
    temp_remind_host.m_message_type = Slave2Host_Out_Focus_Request_Http;
    add_remind_to_queue(temp_remind_host);
}


void EspNowSlave::slave_respense_espnow_mqtt_get_todo_list(const espnow_packet_t& recv_packet)
{
    //-----------------------------------------先判断是否要接这个包-----------------------------------------//
    uint8_t target_mac[ESP_NOW_ETH_ALEN];
    memcpy(target_mac, &recv_packet.data[0], ESP_NOW_ETH_ALEN);

    // 如果目标mac不是本机mac且不是广播mac，则忽略
    if(memcmp(target_mac, get_global_data()->m_mac_uint, ESP_NOW_ETH_ALEN) != 0 && memcmp(target_mac, BROADCAST_MAC, ESP_NOW_ETH_ALEN) != 0)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateTaskList_Control_Mqtt message for MAC: " MACSTR ", ignore", MAC2STR(target_mac));
        return;
    }
    //-----------------------------------------先判断是否要接这个包-----------------------------------------//

    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;

    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    bool clear_flag = recv_packet.data[ESP_NOW_ETH_ALEN + 1];  // 获取清除标志

    // 如果是第一个包且需要清除之前的数据
    if(clear_flag) {
        clean_todo_list(get_global_data()->m_todo_list);
    }

    // 从数据包中解析任务，注意偏移量需要加上MAC地址的长度 
    size_t offset = ESP_NOW_ETH_ALEN + 2; // MAC地址(6字节) + 头部(2字节)
    while(offset < recv_packet.data_len) 
    {
        // 读取任务长度
        size_t title_len = recv_packet.data[offset++];
        
        // 读取任务ID（4字节）
        uint32_t task_id = (recv_packet.data[offset] << 24) | 
                          (recv_packet.data[offset + 1] << 16) |
                          (recv_packet.data[offset + 2] << 8) |
                          recv_packet.data[offset + 3];
        offset += 4;

        // 读取任务内容
        char task_title[50];
        memcpy(task_title, &recv_packet.data[offset], title_len);
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

void EspNowSlave::slave_respense_espnow_mqtt_get_enter_focus(const espnow_packet_t& recv_packet)
{
    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    focus_message_t focus_message = data_to_focus_message((uint8_t*)recv_packet.data);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message, focus type: %d, focus time: %d", focus_message.focus_type, focus_message.focus_time);

    TodoItem todo;
    cleantodoItem(&todo);

    todo.foucs_type = focus_message.focus_type;
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

void EspNowSlave::slave_respense_espnow_mqtt_get_update_recording(const espnow_packet_t& recv_packet)
{ 
    // 获取包序号（2字节）
    uint16_t packet_index = (recv_packet.data[0] << 8) | recv_packet.data[1];
    
    // 处理序号为0的信息包
    if (packet_index == 0) {
        // 解析录音数据总大小（4字节）
        uint32_t total_size = (recv_packet.data[2] << 24) | 
                             (recv_packet.data[3] << 16) | 
                             (recv_packet.data[4] << 8) | 
                             recv_packet.data[5];
        
        // 解析包的总数量（2字节）
        uint16_t total_packets = (recv_packet.data[6] << 8) | recv_packet.data[7];
        
        // 重置录音缓冲区
        MCodec::Instance()->recorded_size = 0;
        MCodec::Instance()->target_record_size = total_size;
        
        ESP_LOGI(ESP_NOW, "Received info packet: total_size=%d, total_packets=%d", 
                 total_size, total_packets);
        return;
    }
    
    // 处理数据包（序号从1开始）
    // 计算实际数据长度（总长度减去序号字节）
    size_t data_length = recv_packet.data_len - 2;  
    // 复制数据到录音缓冲区
    memcpy(&MCodec::Instance()->record_buffer[MCodec::Instance()->recorded_size], 
           &recv_packet.data[2], data_length);
    // 更新已记录的大小
    MCodec::Instance()->recorded_size += data_length;
    ESP_LOGI(ESP_NOW, "Received data packet %d, size: %d, total recorded: %d/%d", 
             packet_index, data_length, MCodec::Instance()->recorded_size, MCodec::Instance()->target_record_size);
}


