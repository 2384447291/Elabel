#include "Esp_now_client.hpp"

uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool Is_connect_to_host(){return EspNowClient::Instance()->is_connect_to_host;}

void set_is_connect_to_host(bool _is_connect_to_host)
{
    EspNowClient::Instance()->is_connect_to_host = _is_connect_to_host;
}


void EspNowClient::print_uint8_array(uint8_t *array, size_t length) {
    // 创建一个字符串缓冲区以存储打印内容
    char buffer[256]; // 假设最大长度为 256 字节
    size_t offset = 0;

    for (size_t i = 0; i < length; i++) {
        // 将每个元素格式化为十六进制并追加到缓冲区
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", array[i]);
    }

    // 使用 ESP_LOGI 打印整个数组
    ESP_LOGI("data", "Array: %s\n", buffer);
}

void EspNowClient::send_esp_now_message(uint8_t target_mac[6], uint8_t* data, size_t len)
{
    // 发送广播数据
    esp_err_t err;
    err = esp_now_send(target_mac, data, len);
    if (err!= ESP_OK) ESP_LOGE(ESP_NOW, "Send espnow message Error occurred: %s", esp_err_to_name(err));
    else 
    {
        ESP_LOGI(ESP_NOW, "Send espnow message success");
        print_uint8_array(data, len);
    }
}

 
void EspNowClient::Addpeer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN])
{
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ESP_NOW, "Malloc peer information fail");
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = peer_chaneel;
    peer->ifidx = WIFI_IF_AP;
    peer->encrypt = false;
    memcpy(peer->peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    ESP_LOGI(ESP_NOW, "Add peer " MACSTR " success", MAC2STR(peer_mac));
    free(peer);
}

void EspNowClient::Delpeer(uint8_t peer_mac[ESP_NOW_ETH_ALEN])
{
    ESP_ERROR_CHECK(esp_now_del_peer(peer_mac));
}

void EspNowClient::ModifyPeer(uint8_t peer_mac[ESP_NOW_ETH_ALEN], uint8_t peer_chaneel)
{
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ESP_NOW, "Malloc peer information fail");
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = peer_chaneel;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_mod_peer(peer));
    free(peer);
}

static void m_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if(status != ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGE(ESP_NOW, "Send espnow message fail to " MACSTR ", status: %d", 
            MAC2STR(mac_addr), 
            status
        );
    }
}

static void m_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (mac_addr == NULL || data == NULL || len <= 0) 
    {
        ESP_LOGE(ESP_NOW, "Receive espnow message cb arg error");
        return;
    }
    // 创建消息结构体并填充数据
    espnow_message_t msg;
    memcpy(msg.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    memcpy(msg.data, data, len);
    msg.data_len = len;

    if (xQueueSend(EspNowClient::Instance()->recv_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(ESP_NOW, "Receive queue fail");
    }
}

void EspNowClient::espnow_update_task(void* parameters)
{
    uint8_t channel = 0;
    const TickType_t channel_switch_delay = pdMS_TO_TICKS(1000);  // 信道切换间隔1秒
    TickType_t last_switch_time = xTaskGetTickCount();

    while (1)
    {
        // 处理接收到的消息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_queue) < MAX_RECV_QUEUE_SIZE)
        {
            espnow_message_t recv_msg;   
            if(xQueueReceive(EspNowClient::Instance()->recv_queue, &recv_msg, 0) == pdTRUE)
            {
                ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_msg.mac_addr), recv_msg.data_len);
                EspNowClient::Instance()->print_uint8_array(recv_msg.data, recv_msg.data_len);
                // 解析广播包
                if(recv_msg.data[0] == 0xAB && recv_msg.data[1] == 0xCD && recv_msg.data[recv_msg.data_len-1] == 0xEF)
                {
                    if(recv_msg.data[2] == Bind_Control_Host2Slave)
                    {
                        size_t username_len = recv_msg.data_len - 5;
                        memcpy(EspNowSlave::Instance()->username, (char*)(recv_msg.data+4), username_len);
                        memcpy(EspNowSlave::Instance()->host_mac, (uint8_t*)(recv_msg.mac_addr), ESP_NOW_ETH_ALEN);
                        EspNowSlave::Instance()->host_channel = recv_msg.data[3];
                        EspNowClient::Instance()->is_connect_to_host = true;
                        ESP_LOGI(ESP_NOW, "Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d", 
                            EspNowSlave::Instance()->username, 
                            MAC2STR(EspNowSlave::Instance()->host_mac), 
                        EspNowSlave::Instance()->host_channel);
                        EspNowClient::Instance()->is_connect_to_host = true;
                    }
                }
            }
        }

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

        vTaskDelay(10);  // 避免占用过多CPU
    }
}

void EspNowClient::start_find_channel() {
    if (update_task_handle == NULL) {
        xTaskCreate(espnow_update_task, "espnow_update_task", 4096, NULL, 10, &update_task_handle);
        ESP_LOGI(ESP_NOW, "Started channel finding task");
    }
    else ESP_LOGI(ESP_NOW, "Channel finding task already started");
}

void EspNowClient::stop_find_channel() {
    if (update_task_handle != NULL) {
        vTaskDelete(update_task_handle);
        update_task_handle = NULL;
        ESP_LOGI(ESP_NOW, "Stopped channel finding task");
    }
    else ESP_LOGI(ESP_NOW, "Channel finding task already stopped");
}

void EspNowClient::init(){
    update_task_handle = NULL;  // 初始化任务句柄为空
    //-----------------------------创建接收队列-----------------------------//
    recv_queue = xQueueCreate(MAX_RECV_QUEUE_SIZE, sizeof(espnow_message_t));
    if (recv_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create receive queue fail");
        return;
    }
    //-----------------------------创建接收队列-----------------------------//


    //-----------------------------初始化esp_now,并配置收包发包函数.-----------------------------//
    ESP_ERROR_CHECK( esp_now_init());
    ESP_ERROR_CHECK( esp_now_register_send_cb(m_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(m_espnow_recv_cb) );
    //-----------------------------初始化esp_now,并配置收包发包函数.-----------------------------//

    
   //当且仅当 ESP32 配置为 STA 模式时，允许其进行休眠。
   //-----------------------------节能模式下的操作-----------------------------//
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
   //-----------------------------节能模式下的操作-----------------------------//
}
