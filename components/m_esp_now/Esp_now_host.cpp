#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_nvs.h"
#include "http.h"
static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        //处理接收到的信息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_packet_queue)<MAX_RECV_PACKET_QUEUE_SIZE)
        {
            espnow_packet_t recv_packet;   
            if(xQueueReceive(EspNowClient::Instance()->recv_packet_queue, &recv_packet, 0) == pdTRUE)
            {
                //处理接收到的信息
                // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_packet.mac_addr), recv_packet.data_len);
                // EspNowClient::Instance()->print_uint8_array(recv_packet.data, recv_packet.data_len);
                if(recv_packet.m_message_type == Feedback_ACK)
                {
                    if(EspNowHost::Instance()->send_state == pinning)
                    {
                        EspNowHost::Instance()->get_online_slave(recv_packet.mac_addr);
                    }
                    else if(EspNowHost::Instance()->send_state == target_sending)
                    {
                        EspNowHost::Instance()->get_feedback_from_online_slave(recv_packet.mac_addr);
                    }
                }
                else if(recv_packet.m_message_type == Slave2Host_Bind_Request_Http)
                {
                    EspNowHost::Instance()->Add_new_slave(recv_packet.mac_addr);
                }
            }
        }
    }
}


static void esp_now_send_update(void *pvParameter)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(Esp_Now_Send_Interval));
        static uint32_t innerclock = 0;
        innerclock+=Esp_Now_Send_Interval;

        // 获取当前正在处理的remind_slave
        remind_slave_t* current_remind = EspNowHost::Instance()->get_current_remind();
        
        if (current_remind != nullptr )
        {
            if(EspNowHost::Instance()->send_state == waiting)
            {
                EspNowHost::Instance()->send_state = pinning;
                current_remind->Online_slave_mac.clear();
                EspNowHost::Instance()->pinning_start_time = xTaskGetTickCount();
                EspNowHost::Instance()->pin_message_unique_id = esp_random() & 0xFFFF;
            }
        }  
        else
        {
            EspNowHost::Instance()->send_state = waiting;
        }

        switch (EspNowHost::Instance()->send_state) 
        {
            case pinning:
                // 检查是否超时或者所有从机都在线
                if ((xTaskGetTickCount() - EspNowHost::Instance()->pinning_start_time >= pdMS_TO_TICKS(PINNING_TIMEOUT_MS)) || 
                    (current_remind->Online_slave_mac.size() >= current_remind->Target_slave_mac.size())) 
                {
                    // 打印pinning情况
                    EspNowHost::Instance()->Print_pin_situation();                   
                    // 切换到target_sending状态
                    EspNowHost::Instance()->send_state = target_sending;
                } 
                else 
                {
                    // 继续执行pin操作
                    uint8_t broadcast_data = 0;
                    EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, 
                        &broadcast_data, 1, 
                        Wakeup_Control_Host2Slave, 
                        true, 
                        EspNowHost::Instance()->pin_message_unique_id);
                }
                break;
            case target_sending:
                //判断是不是所有online从机都反馈了,没收到对应的从机反馈，会把对应的从机从online_slave_mac中删除
                if(current_remind->Online_slave_mac.size() == 0) 
                {
                    EspNowHost::Instance()->finish_current_remind();
                    EspNowHost::Instance()->send_state = waiting;
                }
                //发送实际消息
                EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, 
                    current_remind->data, 
                    current_remind->data_len, 
                    current_remind->m_message_type, 
                    true, 
                    current_remind->unique_id);
                break;
            case waiting:
                if(innerclock % 1000 == 0) {
                    // 发送心跳绑定包
                    EspNowHost::Instance()->send_bind_heartbeat();
                }
                break;
        }
    }
}

void EspNowHost::init()
{
    if(host_recieve_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host already init");
        return;
    }
    EspNowClient::Instance()->m_role = host_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    EspNowClient::Instance()->Addpeer(0, BROADCAST_MAC);
    
    // 创建remind队列
    remind_queue = xQueueCreate(10, sizeof(remind_slave_t));
    if (remind_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Failed to create remind queue!");
        return;
    }
    
    xTaskCreate(esp_now_recieve_update, "esp_now_host_recieve_update_task", 4096, NULL, 1, &host_recieve_update_task_handle);
    xTaskCreate(esp_now_send_update, "esp_now_host_send_update_task", 4096, NULL, 10, &host_send_update_task_handle);
    send_state = waiting;
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(host_recieve_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host not init");
        return;
    }
    EspNowClient::Instance()->m_role = default_role;
    vTaskDelete(host_recieve_update_task_handle);
    vTaskDelete(host_send_update_task_handle);
    host_recieve_update_task_handle = NULL;
    host_send_update_task_handle = NULL;
    
    // 删除remind队列
    if (remind_queue != NULL) {
        vQueueDelete(remind_queue);
        remind_queue = NULL;
    }
    
    EspNowClient::Instance()->Delpeer(BROADCAST_MAC);
    ESP_LOGI(ESP_NOW, "Host deinit success");
}


// void EspNowHost::send_update_task_list()
// {
//     if(send_state != waiting) 
//     {
//         ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
//         return;
//     }
//     TodoList* list = get_global_data()->m_todo_list;
//     if (list == NULL) return;

//     // 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
//     size_t total_len = 0;  // 包头3字节 + 包尾1字节
//     total_len += 1;  // 任务数量1字节
//     // 先计算所有任务的总长度
//     for(int i = 0; i < list->size; i++) {
//         total_len += 1;  // 每个任务的长度信息占1字节
//         total_len += 4;  // 任务ID占4字节
//         total_len += strlen(list->items[i].title);  // 任务内容
//     }

//     uint8_t broadcast_data[total_len];
    
//     broadcast_data[0] = list->size;  // 任务数量
    
//     // 填充任务数据
//     size_t offset = 1;  // 当前写入位置（头部4字节 + ID 1字节）
//     for(int i = 0; i < list->size; i++) {
//         size_t title_len = strlen(list->items[i].title);
//         broadcast_data[offset++] = title_len;  // 写入当前任务的长度
        
//         // 写入任务ID（4字节）
//         broadcast_data[offset++] = (list->items[i].id >> 24) & 0xFF;  // ID高字节
//         broadcast_data[offset++] = (list->items[i].id >> 16) & 0xFF;
//         broadcast_data[offset++] = (list->items[i].id >> 8) & 0xFF;
//         broadcast_data[offset++] = list->items[i].id & 0xFF;  // ID低字节
        
//         memcpy(&broadcast_data[offset], list->items[i].title, title_len);  // 写入任务内容
//         offset += title_len;
//     }

//     // 填充remind_slave结构体
//     empty_remind_slave();
//     remind_slave.data_len = total_len;
//     memcpy(remind_slave.data, broadcast_data, total_len);
//     remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
//     remind_slave.m_message_type = Host2Slave_UpdateTaskList_Control_Mqtt;
//     remind_slave.slave_num_not_feedback = online_slave_num;
//     for(int i = 0; i < online_slave_num; i++)
//     {
//         memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
//     }
//     ESP_LOGI(ESP_NOW, "Load update task list data");
//     send_state = target_sending;
// }

// void EspNowHost::send_out_focus()
// {
//     if(send_state != waiting) 
//     {
//         ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
//         return;
//     }
//     TodoList* list = get_global_data()->m_todo_list;
//     if (list == NULL) return;

//     // 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
//     size_t total_len = 0;  // 包头3字节 + 包尾1字节
//     total_len += 1;  // 任务数量1字节
//     // 先计算所有任务的总长度
//     for(int i = 0; i < list->size; i++) {
//         total_len += 1;  // 每个任务的长度信息占1字节
//         total_len += 4;  // 任务ID占4字节
//         total_len += strlen(list->items[i].title);  // 任务内容
//     }

//     uint8_t broadcast_data[total_len];
    
//     broadcast_data[0] = list->size;  // 任务数量
    
//     // 填充任务数据
//     size_t offset = 1;  // 当前写入位置（头部4字节 + ID 1字节）
//     for(int i = 0; i < list->size; i++) {
//         size_t title_len = strlen(list->items[i].title);
//         broadcast_data[offset++] = title_len;  // 写入当前任务的长度
        
//         // 写入任务ID（4字节）
//         broadcast_data[offset++] = (list->items[i].id >> 24) & 0xFF;  // ID高字节
//         broadcast_data[offset++] = (list->items[i].id >> 16) & 0xFF;
//         broadcast_data[offset++] = (list->items[i].id >> 8) & 0xFF;
//         broadcast_data[offset++] = list->items[i].id & 0xFF;  // ID低字节
        
//         memcpy(&broadcast_data[offset], list->items[i].title, title_len);  // 写入任务内容
//         offset += title_len;
//     }

//     // 填充remind_slave结构体
//     empty_remind_slave();
//     remind_slave.data_len = total_len;
//     memcpy(remind_slave.data, broadcast_data, total_len);
//     remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
//     remind_slave.m_message_type = OutFocus_Control_Host2Slave;
//     remind_slave.slave_num_not_feedback = online_slave_num;
//     for(int i = 0; i < online_slave_num; i++)
//     {
//         memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
//     }
//     ESP_LOGI(ESP_NOW, "Load update task list data");
//     send_state = target_sending;
// }

// void EspNowHost::send_enter_focus(TodoItem _todo_item)
// {
//     if(send_state != waiting) 
//     {
//         ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
//         return;
//     }

//     size_t title_len = strlen(_todo_item.title);
//     // 任务ID(4) + 标题长度(1) + 标题内容(title_len) + 倒计时时间(4)
//     size_t total_len = 4 + 1 + title_len + 4;
//     uint8_t broadcast_data[total_len];
//     size_t offset = 0;

//     // 填充任务ID (4字节)
//     broadcast_data[offset++] = (_todo_item.id >> 24) & 0xFF;  // ID高字节
//     broadcast_data[offset++] = (_todo_item.id >> 16) & 0xFF;
//     broadcast_data[offset++] = (_todo_item.id >> 8) & 0xFF;
//     broadcast_data[offset++] = _todo_item.id & 0xFF;  // ID低字节

//     // 填充任务标题长度和内容
//     broadcast_data[offset++] = title_len;  // 标题长度(1字节)
//     memcpy(&broadcast_data[offset], _todo_item.title, title_len);
//     offset += title_len;

//     // 填充倒计时时间 (4字节)
//     broadcast_data[offset++] = (_todo_item.fallTiming >> 24) & 0xFF;
//     broadcast_data[offset++] = (_todo_item.fallTiming >> 16) & 0xFF;
//     broadcast_data[offset++] = (_todo_item.fallTiming >> 8) & 0xFF;
//     broadcast_data[offset++] = _todo_item.fallTiming & 0xFF;

//     // 填充remind_slave结构体
//     empty_remind_slave();
//     remind_slave.data_len = total_len;
//     memcpy(remind_slave.data, broadcast_data, total_len);
//     remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
//     remind_slave.m_message_type = EnterFocus_Control_Host2Slave;
//     remind_slave.slave_num_not_feedback = online_slave_num;
//     for(int i = 0; i < online_slave_num; i++)
//     {
//         memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
//     }
//     ESP_LOGI(ESP_NOW, "Load update task list data");
//     send_state = target_sending;
// }