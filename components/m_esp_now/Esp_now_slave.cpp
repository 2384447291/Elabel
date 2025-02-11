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
    xTaskCreate(esp_now_recieve_update, "esp_now_slave_recieve_update_task", 4096, NULL, 10, &slave_recieve_update_task_handle);
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
    feedback_data[2] = Bind_feedback_message;
    feedback_data[3] = 0xEF;  // 包尾

    // 发送数据
    this->client->send_esp_now_message(this->host_mac, feedback_data, 4);
}
