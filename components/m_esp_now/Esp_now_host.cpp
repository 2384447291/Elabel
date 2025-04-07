#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_nvs.h"
#include "http.h"
#include "global_time.h"
#include "codec.hpp"

// 添加录音数据发送线程的句柄
static TaskHandle_t recording_send_task_handle = NULL;

// 录音数据发送线程函数
static void recording_send_task(void *pvParameter)
{
    // 获取录音数据大小
    int recorded_size = MCodec::Instance()->recorded_size;
    
    // 计算一共要发多少包，向上取整
    int packet_num = (recorded_size + MAX_EFFECTIVE_DATA_LEN - 1) / MAX_EFFECTIVE_DATA_LEN;
    ESP_LOGI(ESP_NOW, "Recording data size: %d, packet num: %d", recorded_size, packet_num);
    
    // 发送所有数据包
    int current_data_index = 0;
    for(int i = 0; i < packet_num; i++)
    {
        // 计算当前包的数据大小
        int current_packet_size = (i == packet_num - 1) ? 
            (recorded_size - current_data_index) : 
            MAX_EFFECTIVE_DATA_LEN;
        
        // 准备当前数据包
        uint8_t current_packet[MAX_EFFECTIVE_DATA_LEN];
        // 第一个字节存储包序号
        current_packet[0] = i;
        // 复制实际数据
        memcpy(&current_packet[1], &MCodec::Instance()->record_buffer[current_data_index], current_packet_size - 1);

        // 填充remind_slave结构体
        remind_slave_t remind_slave;
        remind_slave.data_len = current_packet_size;
        memcpy(remind_slave.data, current_packet, current_packet_size);
        remind_slave.unique_id = esp_random() & 0xFFFF;
        remind_slave.m_message_type = Host2Slave_UpdateRecording_Control_Mqtt;

        // 填充Target_slave_mac
        remind_slave.Target_slave_mac.clear();
        remind_slave.Target_slave_mac = EspNowHost::Instance()->Bind_slave_mac;

        // 清除Online_slave_mac
        remind_slave.Online_slave_mac.clear();

        // 尝试添加remind_slave到队列，如果队列满则等待
        bool added = false;
        while (!added) {
            added = EspNowHost::Instance()->add_remind_to_queue(remind_slave);
            if (!added) {
                vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            }
        }

        // 更新当前数据索引
        current_data_index += (current_packet_size - 1);
        
        ESP_LOGI(ESP_NOW, "Sent packet %d/%d, size: %d", i+1, packet_num, current_packet_size);
    }
    
    ESP_LOGI(ESP_NOW, "All recording data sent successfully");
    
    // 任务完成后删除自身
    recording_send_task_handle = NULL;
    vTaskDelete(NULL);
}

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
                    uint16_t feedback_unique_id = (recv_packet.data[0] << 8) | recv_packet.data[1];
                    if(EspNowHost::Instance()->send_state == pinning)
                    {
                        if(EspNowHost::Instance()->pin_message_unique_id == feedback_unique_id)
                        {
                            ESP_LOGI(ESP_NOW, "Get online slave from " MACSTR " ,unique_id: %d", MAC2STR(recv_packet.mac_addr), feedback_unique_id);
                            EspNowHost::Instance()->get_online_slave(recv_packet.mac_addr);
                        }
                    }
                    else if(EspNowHost::Instance()->send_state == target_sending)
                    {
                        if(EspNowHost::Instance()->current_remind.unique_id == feedback_unique_id)
                        {
                            ESP_LOGI(ESP_NOW, "Get feedback from " MACSTR " ,unique_id: %d", MAC2STR(recv_packet.mac_addr), feedback_unique_id);
                            EspNowHost::Instance()->get_feedback_from_online_slave(recv_packet.mac_addr);
                        }
                    }
                }
                else if(recv_packet.m_message_type == Slave2Host_Bind_Request_Http)
                {
                    ESP_LOGI(ESP_NOW, "Receive Slave2Host_Bind_Request_Http from " MACSTR, MAC2STR(recv_packet.mac_addr));
                    EspNowHost::Instance()->Add_new_slave(recv_packet.mac_addr);
                }
                else if(recv_packet.m_message_type == Slave2Host_UpdateTaskList_Request_Http)
                {
                    ESP_LOGI(ESP_NOW, "Receive Slave2Host_UpdateTaskList_Request_Http from " MACSTR, MAC2STR(recv_packet.mac_addr));
                    EspNowHost::Instance()->Mqtt_update_task_list(recv_packet.mac_addr, false);
                }
                else if(recv_packet.m_message_type == Slave2Host_Enter_Focus_Request_Http)
                {
                    ESP_LOGI(ESP_NOW, "Receive Slave2Host_Enter_Focus_Request_Http from " MACSTR, MAC2STR(recv_packet.mac_addr));
                    EspNowHost::Instance()->http_response_enter_focus(recv_packet);
                }
                else if(recv_packet.m_message_type == Slave2Host_Out_Focus_Request_Http)
                {
                    ESP_LOGI(ESP_NOW, "Receive Slave2Host_Out_Focus_Request_Http from " MACSTR, MAC2STR(recv_packet.mac_addr));
                    EspNowHost::Instance()->http_response_out_focus(recv_packet);
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

        if(EspNowHost::Instance()->send_state == waiting)
        {
            if(EspNowHost::Instance()->get_current_remind(&EspNowHost::Instance()->current_remind))
            {
                EspNowHost::Instance()->send_state = pinning;
                EspNowHost::Instance()->current_remind.Online_slave_mac.clear();
                EspNowHost::Instance()->pinning_start_time = xTaskGetTickCount();
                EspNowHost::Instance()->pin_message_unique_id = esp_random() & 0xFFFF;
            }  
        }

        switch (EspNowHost::Instance()->send_state) 
        {
            case pinning:
                // 检查是否超时或者所有从机都在线
                if ((xTaskGetTickCount() - EspNowHost::Instance()->pinning_start_time >= pdMS_TO_TICKS(PINNING_TIMEOUT_MS)) || 
                    (EspNowHost::Instance()->current_remind.Online_slave_mac.count == EspNowHost::Instance()->current_remind.Target_slave_mac.count)) 
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
                if(EspNowHost::Instance()->current_remind.Online_slave_mac.count == 0) 
                {
                    EspNowHost::Instance()->finish_current_remind();
                    EspNowHost::Instance()->send_state = waiting;
                }
                //发送实际消息
                EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, 
                    EspNowHost::Instance()->current_remind.data, 
                    EspNowHost::Instance()->current_remind.data_len, 
                    EspNowHost::Instance()->current_remind.m_message_type, 
                    true, 
                    EspNowHost::Instance()->current_remind.unique_id);
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

    //更新从机列表
    Bind_slave_mac.clear();
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        Bind_slave_mac.insert(get_global_data()->m_slave_mac[i]);
    }
    
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

void EspNowHost::http_response_enter_focus(const espnow_packet_t& recv_packet)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message((uint8_t*)recv_packet.data);
    if(focus_message.focus_type == 1)
    {
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        TodoItem todo;
        cleantodoItem(&todo);

        todo.foucs_type = 1;
        todo.fallTiming = focus_message.focus_time;
        todo.id = focus_message.focus_id;
        todo.isFocus = 1;
        todo.startTime = get_unix_time();
        char title[20] = "Pure Time Task";
        todo.title = title;
        
        clean_todo_list(get_global_data()->m_todo_list);
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
        set_task_list_state(firmware_need_update);
    }
    else if(focus_message.focus_type == 2 || focus_message.focus_type == 0)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_in_focus(sstr,focus_message.focus_time,false);
    }
    else if(focus_message.focus_type == 3)
    {
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        TodoItem todo;
        cleantodoItem(&todo);

        todo.foucs_type = 3;
        todo.fallTiming = focus_message.focus_time;
        todo.id = focus_message.focus_id;
        todo.isFocus = 1;
        todo.startTime = get_unix_time();
        char title[20] = "Record Task";
        todo.title = title;
        
        clean_todo_list(get_global_data()->m_todo_list);
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
        set_task_list_state(firmware_need_update);
    }
}

void EspNowHost::http_response_out_focus(const espnow_packet_t& recv_packet)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message((uint8_t*)recv_packet.data);
 
    if(focus_message.focus_type == 2 || focus_message.focus_type == 0)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_out_focus(sstr,false);
    }
    //因为这两个bro都没上传到服务器所以不用out_focus
    else if(focus_message.focus_type == 1)
    {
        //模拟http+mqtt的out_focus
        get_global_data()->m_focus_state->is_focus = 2;
        get_global_data()->m_focus_state->focus_task_id = 0;
        http_get_todo_list(false);
    }
    else if(focus_message.focus_type == 3)
    {
        //模拟http+mqtt的out_focus
        get_global_data()->m_focus_state->is_focus = 2;
        get_global_data()->m_focus_state->focus_task_id = 0;
        http_get_todo_list(false);
    } 
}

void EspNowHost::Mqtt_update_recording()
{
    // 检查是否已经有录音发送任务在运行
    if (recording_send_task_handle != NULL) {
        ESP_LOGW(ESP_NOW, "Recording send task already running");
        return;
    }
    
    // 检查是否有录音数据
    if(MCodec::Instance()->recorded_size == 0) 
    {
        ESP_LOGI(ESP_NOW, "No recording data");
        return;
    }
    
    // 创建录音数据发送线程
    BaseType_t result = xTaskCreate(
        recording_send_task,
        "recording_send_task",
        4096,
        NULL,
        10,
        &recording_send_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(ESP_NOW, "Failed to create recording send task");
        return;
    }
    
    ESP_LOGI(ESP_NOW, "Recording send task created successfully");
}

void EspNowHost::Mqtt_update_task_list(uint8_t target_mac[ESP_NOW_ETH_ALEN], bool need_broadcast)
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;
    
    // 发送所有数据包
    int current_task_index = 0;
    while(current_task_index < list->size) {
        // 计算当前包可以包含的任务数量
        size_t current_packet_len = 2;  // 头部2字节（任务总数和清除标志）
        int tasks_in_packet = 0;
        
        // 尝试添加任务，直到再添加一个就会超过最大长度
        while(current_task_index + tasks_in_packet < list->size) {
            size_t next_task_size = 1;  // 任务长度
            next_task_size += 4;  // 任务ID
            next_task_size += strlen(list->items[current_task_index + tasks_in_packet].title);  // 任务内容
            
            // 检查添加下一个任务是否会超过最大长度
            if(current_packet_len + next_task_size > MAX_EFFECTIVE_DATA_LEN) {
                break;
            }
            
            current_packet_len += next_task_size;
            tasks_in_packet++;
        }
        
        // 填充当前包的数据
        uint8_t current_packet[MAX_EFFECTIVE_DATA_LEN];

        if(need_broadcast)
        {
            for(int i = 0; i < ESP_NOW_ETH_ALEN; i++)
            {
                current_packet[i] = BROADCAST_MAC[i];
            }
        }
        else
        {
            for(int i = 0; i < ESP_NOW_ETH_ALEN; i++)
            {
                current_packet[i] = target_mac[i];
            }
        }

        current_packet[ESP_NOW_ETH_ALEN] = list->size;  // 任务总数
        current_packet[ESP_NOW_ETH_ALEN + 1] = (current_task_index == 0) ? true : false;  // 第一个包清除之前的数据，其他包不清除

        // 填充任务数据
        size_t offset = 2 + ESP_NOW_ETH_ALEN;  // 当前写入位置（头部2字节 + 目标mac地址）
        for(int i = 0; i < tasks_in_packet; i++) {
            size_t title_len = strlen(list->items[current_task_index + i].title);
            current_packet[offset++] = title_len;  // 写入当前任务的长度
            
            // 写入任务ID（4字节）
            current_packet[offset++] = (list->items[current_task_index + i].id >> 24) & 0xFF;  // ID高字节
            current_packet[offset++] = (list->items[current_task_index + i].id >> 16) & 0xFF;
            current_packet[offset++] = (list->items[current_task_index + i].id >> 8) & 0xFF;
            current_packet[offset++] = list->items[current_task_index + i].id & 0xFF;  // ID低字节
            
            memcpy(&current_packet[offset], list->items[current_task_index + i].title, title_len);  // 写入任务内容
            offset += title_len;
        }

        // 填充remind_slave结构体
        remind_slave_t remind_slave;
        remind_slave.data_len = offset;
        memcpy(remind_slave.data, current_packet, offset);
        remind_slave.unique_id = esp_random() & 0xFFFF;
        remind_slave.m_message_type = Host2Slave_UpdateTaskList_Control_Mqtt;

        // 填充Target_slave_mac
        remind_slave.Target_slave_mac.clear();
        if(need_broadcast)
        {
            remind_slave.Target_slave_mac = EspNowHost::Instance()->Bind_slave_mac;
        }
        else
        {
            remind_slave.Target_slave_mac.insert(target_mac);
        }

        // 清除Online_slave_mac
        remind_slave.Online_slave_mac.clear();

        // 添加remind_slave到队列
        add_remind_to_queue(remind_slave);

        // 更新当前任务索引
        current_task_index += tasks_in_packet;
    }
}

void EspNowHost::Mqtt_enter_focus(focus_message_t focus_message)
{
    // 填充remind_slave结构体
    remind_slave_t remind_slave;
    focus_message_to_data(focus_message, remind_slave.data, remind_slave.data_len);
    remind_slave.unique_id = esp_random() & 0xFFFF;
    remind_slave.m_message_type = Host2Slave_Enter_Focus_Control_Mqtt;

    // 填充Target_slave_mac
    remind_slave.Target_slave_mac.clear();
    remind_slave.Target_slave_mac = EspNowHost::Instance()->Bind_slave_mac;
    // 清除Online_slave_mac
    remind_slave.Online_slave_mac.clear();
    // 添加remind_slave到队列
    add_remind_to_queue(remind_slave);
}

void EspNowHost::Mqtt_out_focus()
{
    // 填充remind_slave 结构体
    remind_slave_t remind_slave;
    remind_slave.data_len = 1;
    remind_slave.data[0] = 0;
    remind_slave.unique_id = esp_random() & 0xFFFF;
    remind_slave.m_message_type = Host2Slave_Out_Focus_Control_Mqtt;

    // 填充Target_slave_mac
    remind_slave.Target_slave_mac.clear();
    remind_slave.Target_slave_mac = EspNowHost::Instance()->Bind_slave_mac;
    // 清除Online_slave_mac
    remind_slave.Online_slave_mac.clear();
    // 添加remind_slave到队列
    add_remind_to_queue(remind_slave);
}

