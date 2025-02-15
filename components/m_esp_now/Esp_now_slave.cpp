#include "Esp_now_client.hpp"

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(10);
        //处理接收到的信息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_queue)<MAX_RECV_QUEUE_SIZE)
        {
            espnow_message_t  recv_msg;   
            if(xQueueReceive(EspNowClient::Instance()->recv_queue, &recv_msg, 0) == pdTRUE)
            {
                //处理接收到的信息
                ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_msg.mac_addr), recv_msg.data_len);
                EspNowClient::Instance()->print_uint8_array(recv_msg.data, recv_msg.data_len);
                if(recv_msg.data[0] == 0xAB && recv_msg.data[1] == 0xCD && recv_msg.data[recv_msg.data_len-1] == 0xEF)
                {
                    //回复主机已近收到包了
                    EspNowSlave::Instance()->send_feedcak_pack(recv_msg.data[3]);
                    if(EspNowSlave::Instance()->unique_id == recv_msg.data[3]) continue;
                    if(recv_msg.data[2] == UpdateTaskList_Control_Host2Slave || recv_msg.data[2] == OutFocus_Control_Host2Slave)
                    {
                        //清除所有的任务记录
                        clean_todo_list(get_global_data()->m_todo_list);
                        // 保存包的唯一ID
                        EspNowSlave::Instance()->unique_id = recv_msg.data[3];
                        // 获取任务数量
                        uint8_t task_count = recv_msg.data[4];
                        
                        // 清空当前任务列表
                        clean_todo_list(get_global_data()->m_todo_list);
                        
                        // 解析每个任务
                        size_t offset = 5;  // 从数据部分开始
                        for(int i = 0; i < task_count; i++) 
                        {
                            // 读取任务标题长度
                            uint8_t title_len = recv_msg.data[offset++];
                            
                            // 读取任务ID（4字节）
                            uint32_t task_id = (recv_msg.data[offset] << 24) |
                                             (recv_msg.data[offset + 1] << 16) |
                                             (recv_msg.data[offset + 2] << 8) |
                                             recv_msg.data[offset + 3];
                            offset += 4;
                            
                            // 创建任务项
                            TodoItem todo;
                            cleantodoItem(&todo);
                            
                            // 复制任务标题
                            char title[title_len + 1];
                            memcpy(title, &recv_msg.data[offset], title_len);
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
                        if(recv_msg.data[2] == OutFocus_Control_Host2Slave)
                        {
                            set_task_list_state(firmware_need_update);
                            get_global_data()->m_focus_state->is_focus = 2;
                            get_global_data()->m_focus_state->focus_task_id = 0;
                        }
                        else if(recv_msg.data[2] == UpdateTaskList_Control_Host2Slave)
                        {
                            set_task_list_state(firmware_need_update);
                            get_global_data()->m_focus_state->is_focus = 0;
                            get_global_data()->m_focus_state->focus_task_id = 0;
                        }
                    }
                    else if(recv_msg.data[2] == EnterFocus_Control_Host2Slave)
                    {
                        uint8_t offset = 4;
                        //清除所有的任务记录
                        clean_todo_list(get_global_data()->m_todo_list);
                        
                        // 读取任务ID（4字节）
                        uint32_t task_id = (recv_msg.data[offset] << 24) |
                                            (recv_msg.data[offset + 1] << 16) |
                                            (recv_msg.data[offset + 2] << 8) |
                                            recv_msg.data[offset + 3];
                        offset += 4;

                        // 读取任务标题长度
                        uint8_t title_len = recv_msg.data[offset++];
                        
                        // 创建任务项
                        TodoItem todo;
                        cleantodoItem(&todo);
                        
                        // 复制任务标题
                        char title[title_len + 1];
                        memcpy(title, &recv_msg.data[offset], title_len);
                        title[title_len] = '\0';  // 添加字符串结束符
                        todo.title = title;
                        offset += title_len;

                        // 读取倒计时时间（4字节）
                        uint32_t fall_time = (recv_msg.data[offset] << 24) |
                                             (recv_msg.data[offset + 1] << 16) |
                                             (recv_msg.data[offset + 2] << 8) |
                                             recv_msg.data[offset + 3];
                        offset += 4;
                        
                        // 设置任务属性
                        todo.id = task_id;
                        todo.isComplete = 0;
                        todo.isFocus = 0;
                        todo.isImportant = 0;
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
}

void EspNowSlave::init()
{
    if(slave_recieve_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave already init");
        return;
    }
    this->client->m_role = slave_role;
    //停止搜索设备
    this->client->stop_find_channel();
    //设置信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    this->client->Addpeer(this->host_channel, this->host_mac);
    xTaskCreate(esp_now_recieve_update, "esp_now_slave_recieve_update_task", 4096, NULL, 1, &slave_recieve_update_task_handle);
    ESP_LOGI(ESP_NOW, "Slave init success");
}

void EspNowSlave::deinit()
{
    if(slave_recieve_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Slave not init");
        return;
    }
    this->client->m_role = default_role;
    vTaskDelete(slave_recieve_update_task_handle);
    slave_recieve_update_task_handle = NULL;
    this->client->Delpeer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

void EspNowSlave::send_feedback_message()
{
    uint8_t feedback_data[4];
    
    // 填充数据包头部
    feedback_data[0] = 0xAB;  // 包头
    feedback_data[1] = 0xCD;  // 包头
    feedback_data[2] = Bind_Feedback_Slave2Host;
    feedback_data[3] = 0xEF;  // 包尾

    // 发送数据
    this->client->send_esp_now_message(this->host_mac, feedback_data, 4);
}

void EspNowSlave::send_out_focus_message(int task_id, uint8_t unique_id)
{
    uint8_t out_focus_data[9];
    out_focus_data[0] = 0xAB;
    out_focus_data[1] = 0xCD;
    out_focus_data[2] = OutFocus_Control_Slave2Host;
    out_focus_data[3] = unique_id;
    out_focus_data[4] = (task_id >> 24) & 0xFF;
    out_focus_data[5] = (task_id >> 16) & 0xFF;
    out_focus_data[6] = (task_id >> 8) & 0xFF;
    out_focus_data[7] = task_id & 0xFF;
    out_focus_data[8] = 0xEF;
    
    //发送数据
    this->client->send_esp_now_message(this->host_mac, out_focus_data, 9);
}

void EspNowSlave::send_enter_focus_message(int task_id, int fall_time, uint8_t unique_id)
{
    uint8_t enter_focus_data[13];
    enter_focus_data[0] = 0xAB;
    enter_focus_data[1] = 0xCD;
    enter_focus_data[2] = EnterFocus_Control_Slave2Host;
    enter_focus_data[3] = unique_id;
    enter_focus_data[4] = (task_id >> 24) & 0xFF;
    enter_focus_data[5] = (task_id >> 16) & 0xFF;
    enter_focus_data[6] = (task_id >> 8) & 0xFF;
    enter_focus_data[7] = task_id & 0xFF;
    enter_focus_data[8] = (fall_time >> 24) & 0xFF;
    enter_focus_data[9] = (fall_time >> 16) & 0xFF;
    enter_focus_data[10] = (fall_time >> 8) & 0xFF;
    enter_focus_data[11] = fall_time & 0xFF;
    enter_focus_data[12] = 0xEF;

    //发送数据
    this->client->send_esp_now_message(this->host_mac, enter_focus_data, 13);
}
