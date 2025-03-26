#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(xTaskGetTickCount() - EspNowSlave::Instance()->last_recv_heart_time > pdMS_TO_TICKS(5000) && EspNowSlave::Instance()->last_recv_heart_time != 0)
        {
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
                //如果收到和当前发送消息对应的feedback，则完成一次发送
                if(recv_packet.m_message_type == Feedback_ACK)
                {
                    uint16_t feedback_unique_id = (recv_packet.data[0] << 8) | recv_packet.data[1];
                    if(feedback_unique_id == EspNowSlave::Instance()->remind_host.unique_id)
                    {
                        EspNowSlave::Instance()->need_send_data = false;
                    }
                }
                if(recv_packet.m_message_type == UpdateTaskList_Control_Host2Slave || recv_packet.m_message_type == OutFocus_Control_Host2Slave)
                {
                    //清除所有的任务记录
                    clean_todo_list(get_global_data()->m_todo_list);

                    // 获取任务数量
                    uint8_t task_count = recv_packet.data[0];
                    
                    // 解析每个任务
                    size_t offset = 1;  // 从数据部分开始
                    for(int i = 0; i < task_count; i++) 
                    {
                        // 读取任务标题长度
                        uint8_t title_len = recv_packet.data[offset++];
                        
                        // 读取任务ID（4字节）
                        uint32_t task_id = (recv_packet.data[offset] << 24) | (recv_packet.data[offset + 1] << 16) 
                        | (recv_packet.data[offset + 2] << 8) | recv_packet.data[offset + 3];
                        offset += 4;
    
                        // 创建任务项
                        TodoItem todo;
                        cleantodoItem(&todo);
                        
                        // 复制任务标题
                        char title[title_len + 1];
                        memcpy(title, &recv_packet.data[offset], title_len);
                        title[title_len] = '\0';  // 添加字符串结束符
                        todo.title = title;
                        offset += title_len;
                        
                        // 设置任务属性
                        todo.id = task_id;
                        todo.isComplete = 0;
                        todo.isFocus = 0;
                        todo.isImportant = 0;
                        
                        // 添加到任务列表
                        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
                    }
                    if(recv_packet.m_message_type == OutFocus_Control_Host2Slave)
                    {
                        set_task_list_state(firmware_need_update);
                        get_global_data()->m_focus_state->is_focus = 2;
                        get_global_data()->m_focus_state->focus_task_id = 0;
                    }
                    else if(recv_packet.m_message_type == UpdateTaskList_Control_Host2Slave)
                    {
                        set_task_list_state(firmware_need_update);
                        get_global_data()->m_focus_state->is_focus = 0;
                        get_global_data()->m_focus_state->focus_task_id = 0;
                    }
                }
                else if(recv_packet.m_message_type == EnterFocus_Control_Host2Slave)
                {
                    //清除所有的任务记录
                    clean_todo_list(get_global_data()->m_todo_list);
                    
                    // 读取任务ID（4字节）
                    uint32_t task_id = (recv_packet.data[0] << 24) | (recv_packet.data[1] << 16) 
                    | (recv_packet.data[2] << 8) | recv_packet.data[3];

                    // 读取任务标题长度
                    uint8_t title_len = recv_packet.data[4];
                    
                    // 创建任务项
                    TodoItem todo;
                    cleantodoItem(&todo);
                    
                    // 复制任务标题
                    char title[title_len + 1];
                    memcpy(title, &recv_packet.data[5], title_len);
                    title[title_len] = '\0';  // 添加字符串结束符

                    // 读取倒计时时间（4字节）
                    uint32_t fall_time = (recv_packet.data[9] << 24) | (recv_packet.data[10] << 16) 
                    | (recv_packet.data[11] << 8) | recv_packet.data[12];
                    
                    // 设置任务属性
                    todo.isFocus = 1;
                    todo.id = task_id;
                    todo.title = title;
                    todo.fallTiming = fall_time;

                    // 添加到任务列表
                    add_or_update_todo_item(get_global_data()->m_todo_list, todo);
                    set_task_list_state(firmware_need_update);
                    get_global_data()->m_focus_state->is_focus = 1;
                    get_global_data()->m_focus_state->focus_task_id = task_id;
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
        //如果有要发送的remind消息
        if(EspNowSlave::Instance()->need_send_data)
        {
            //发送remind消息
            EspNowClient::Instance()->send_esp_now_message(EspNowSlave::Instance()->host_mac, EspNowSlave::Instance()->remind_host.data, EspNowSlave::Instance()->remind_host.data_len, EspNowSlave::Instance()->remind_host.m_message_type, true, EspNowSlave::Instance()->remind_host.unique_id);
        }
    }
}

void EspNowSlave::init()
{
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
    EspNowClient::Instance()->Delpeer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

void EspNowSlave::send_bind_message()
{
    if(need_send_data)
    {
        ESP_LOGI(ESP_NOW, "Slave send bind message failed, need_send_data is true");
        return;
    }
    uint8_t m_bind_data[1];
    m_bind_data[0] = 0x00;
    remind_host.unique_id = random()&&0xFFFF;
    remind_host.m_message_type = Bind_Feedback_Slave2Host;
    remind_host.data_len = 1;
    memcpy(remind_host.data, m_bind_data, 1);
    need_send_data = true;
}

void EspNowSlave::send_out_focus_message(int task_id)
{
    if(need_send_data)
    {
        ESP_LOGI(ESP_NOW, "Slave send out focus message failed, need_send_data is true");
        return;
    }
    uint8_t out_focus_data[4];
    out_focus_data[0] = (task_id >> 24) & 0xFF;
    out_focus_data[1] = (task_id >> 16) & 0xFF;
    out_focus_data[2] = (task_id >> 8) & 0xFF;
    out_focus_data[3] = task_id & 0xFF;
    remind_host.unique_id = random()&&0xFFFF;
    remind_host.m_message_type = OutFocus_Control_Slave2Host;
    remind_host.data_len = 4;
    memcpy(remind_host.data, out_focus_data, 4);
    need_send_data = true;
}

void EspNowSlave::send_enter_focus_message(int task_id, int fall_time)
{
    if(need_send_data)
    {
        ESP_LOGI(ESP_NOW, "Slave send enter focus message failed, need_send_data is true");
        return;
    }
    uint8_t enter_focus_data[8];
    enter_focus_data[0] = (task_id >> 24) & 0xFF;
    enter_focus_data[1] = (task_id >> 16) & 0xFF;
    enter_focus_data[2] = (task_id >> 8) & 0xFF;
    enter_focus_data[3] = task_id & 0xFF;
    enter_focus_data[4] = (fall_time >> 24) & 0xFF;
    enter_focus_data[5] = (fall_time >> 16) & 0xFF;
    enter_focus_data[6] = (fall_time >> 8) & 0xFF;
    enter_focus_data[7] = fall_time & 0xFF;
    remind_host.unique_id = random()&&0xFFFF;
    remind_host.m_message_type = EnterFocus_Control_Slave2Host;
    remind_host.data_len = 8;
    memcpy(remind_host.data, enter_focus_data, 8);
    need_send_data = true;
}
