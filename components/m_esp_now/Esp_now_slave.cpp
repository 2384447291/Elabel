#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(xTaskGetTickCount() - EspNowSlave::Instance()->last_recv_heart_time > pdMS_TO_TICKS(5000) && EspNowSlave::Instance()->last_recv_heart_time != 0)
        {
            EspNowSlave::Instance()->set_host_status(false);
            ESP_LOGE(ESP_NOW, "Slave not receive heart from host, reconnect");
        }
        else
        {
            EspNowSlave::Instance()->set_host_status(true);
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
                    if(feedback_unique_id == EspNowSlave::Instance()->remind_host.unique_id)
                    {
                        EspNowSlave::Instance()->need_send_data = false;
                        EspNowSlave::Instance()->finish_current_remind();
                    }
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
        vTaskDelay(pdMS_TO_TICKS(Esp_Now_Send_Interval));
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
        //如果有要发送的remind消息
        // 获取当前正在处理的remind_slave
        remind_host_t* current_remind = EspNowSlave::Instance()->get_current_remind();
        
        if (current_remind != nullptr)
        {
            if(!EspNowSlave::Instance()->need_send_data)
            {
                EspNowSlave::Instance()->need_send_data = true;
            }
        } 
        else
        {
            EspNowSlave::Instance()->need_send_data = false;
        }

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

// void EspNowSlave::send_out_focus_message(int task_id)
// {
//     if(need_send_data)
//     {
//         ESP_LOGI(ESP_NOW, "Slave send out focus message failed, need_send_data is true");
//         return;
//     }
//     uint8_t out_focus_data[4];
//     out_focus_data[0] = (task_id >> 24) & 0xFF;
//     out_focus_data[1] = (task_id >> 16) & 0xFF;
//     out_focus_data[2] = (task_id >> 8) & 0xFF;
//     out_focus_data[3] = task_id & 0xFF;
//     remind_host.unique_id = random()&&0xFFFF;
//     remind_host.m_message_type = OutFocus_Control_Slave2Host;
//     remind_host.data_len = 4;
//     memcpy(remind_host.data, out_focus_data, 4);
//     need_send_data = true;
// }

// void EspNowSlave::send_enter_focus_message(int task_id, int fall_time)
// {
//     if(need_send_data)
//     {
//         ESP_LOGI(ESP_NOW, "Slave send enter focus message failed, need_send_data is true");
//         return;
//     }
//     uint8_t enter_focus_data[8];
//     enter_focus_data[0] = (task_id >> 24) & 0xFF;
//     enter_focus_data[1] = (task_id >> 16) & 0xFF;
//     enter_focus_data[2] = (task_id >> 8) & 0xFF;
//     enter_focus_data[3] = task_id & 0xFF;
//     enter_focus_data[4] = (fall_time >> 24) & 0xFF;
//     enter_focus_data[5] = (fall_time >> 16) & 0xFF;
//     enter_focus_data[6] = (fall_time >> 8) & 0xFF;
//     enter_focus_data[7] = fall_time & 0xFF;
//     remind_host.unique_id = random()&&0xFFFF;
//     remind_host.m_message_type = EnterFocus_Control_Slave2Host;
//     remind_host.data_len = 8;
//     memcpy(remind_host.data, enter_focus_data, 8);
//     need_send_data = true;
// }

void EspNowSlave::slave_espnow_http_get_todo_list()
{
    remind_host.data[0] = 0x00;
    remind_host.data_len = 1;
    remind_host.unique_id = random()&&0xFFFF;
    remind_host.m_message_type = Slave2Host_UpdateTaskList_Request_Http;
    add_remind_to_queue(remind_host);
}

void EspNowSlave::slave_espnow_http_bind_host_request()
{
    remind_host.data[0] = 0x00;
    remind_host.data_len = 1;
    remind_host.unique_id = random()&&0xFFFF;
    remind_host.m_message_type = Slave2Host_Bind_Request_Http;
    add_remind_to_queue(remind_host);
}


