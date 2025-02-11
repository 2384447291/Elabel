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
                    if(recv_msg.data[2] == Bind_feedback_message)
                    {
                        EspNowHost::Instance()->add_slave(recv_msg.mac_addr);
                    }
                }
            }
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
    this->client->m_role = host_role;
    //停止搜索设备
    this->client->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    uint8_t mac_addr[6] = {0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF};
    this->client->Addpeer(0, mac_addr);
    xTaskCreate(esp_now_recieve_update, "esp_now_host_recieve_update_task", 4096, NULL, 10, &host_recieve_update_task_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(host_recieve_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host not init");
        return;
    }
    this->client->m_role = default_role;
    vTaskDelete(host_recieve_update_task_handle);
    host_recieve_update_task_handle = NULL;
    uint8_t mac_addr[6] = {0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF};
    this->client->Delpeer(mac_addr);
    ESP_LOGI(ESP_NOW, "Host deinit success");
}

void EspNowHost::add_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN])
{
    // 首先检查是否已经存在该MAC地址
    for(int i = 0; i < slave_num; i++)
    {
        if(memcmp(slave_mac[i], _slave_mac, ESP_NOW_ETH_ALEN) == 0)
        {
            ESP_LOGI(ESP_NOW, "Slave " MACSTR " already exists", MAC2STR(_slave_mac));
            return;
        }
    }

    // 检查是否超过最大从机数量限制
    if(slave_num >= MAX_SLAVE_NUM)
    {
        ESP_LOGI(ESP_NOW, "Slave number exceeds limit %d", MAX_SLAVE_NUM);
        return;
    }

    // 添加新的从机MAC地址
    memcpy(slave_mac[slave_num], _slave_mac, ESP_NOW_ETH_ALEN);
    slave_num++;
    ESP_LOGI(ESP_NOW, "Add slave " MACSTR " success", MAC2STR(_slave_mac));
}

void EspNowHost::del_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN])
{
    for(int i = 0; i < slave_num; i++)
    {
        if(memcmp(slave_mac[i], _slave_mac, ESP_NOW_ETH_ALEN) == 0)
        {
            // 找到匹配的MAC地址,将后面的MAC地址前移
            for(int j = i; j < slave_num - 1; j++)
            {
                memcpy(slave_mac[j], slave_mac[j+1], ESP_NOW_ETH_ALEN);
            }
            slave_num--;
            ESP_LOGI(ESP_NOW, "Delete slave " MACSTR " success", MAC2STR(_slave_mac));
            return;
        }
    }
    ESP_LOGI(ESP_NOW, "Slave " MACSTR " not found", MAC2STR(_slave_mac));
}




void EspNowHost::send_broadcast_message()
{
    uint8_t wifi_channel = 0;
    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

    // 计算数据包总长度：头部(4字节) + MAC(6字节) + 用户名长度
    size_t username_len = strlen(get_global_data()->m_userName);
    size_t total_len = 5 + username_len;
    uint8_t broadcast_data[total_len];
    
    // 填充数据包头部
    broadcast_data[0] = 0xAB;  // 包头
    broadcast_data[1] = 0xCD;  // 包头
    broadcast_data[2] = Bind_broadcast_message;
    broadcast_data[3] = wifi_channel;  // WiFi通道
    broadcast_data[total_len-1] = 0xEF;  // 包尾
    
    // 填充用户名
    memcpy(&broadcast_data[4], get_global_data()->m_userName, username_len);

    // 发送数据
    uint8_t mac_addr[6] = {0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF};
    this->client->send_esp_now_message(mac_addr, broadcast_data, total_len);
}


