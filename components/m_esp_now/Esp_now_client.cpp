#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "esp_now_host.hpp"

uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool Is_connect_to_host(){return EspNowClient::Instance()->is_connect_to_host;}

void set_is_connect_to_host(bool _is_connect_to_host)
{
    EspNowClient::Instance()->is_connect_to_host = _is_connect_to_host;
}


focus_message_t pack_focus_message(uint8_t focus_type, uint16_t focus_time, int focus_id, char* task_name)
{
    focus_message_t focus_message;
    focus_message.focus_type = focus_type;
    focus_message.focus_time = focus_time;
    focus_message.focus_id = focus_id;
    focus_message.task_name_len = strlen(task_name);
    memcpy(focus_message.task_name, task_name, focus_message.task_name_len);
    return focus_message;
}

void focus_message_to_data(focus_message_t focus_message, uint8_t* data, size_t &data_len)
{
    data[0] = focus_message.focus_type;
    data[1] = focus_message.focus_time >> 8;
    data[2] = focus_message.focus_time & 0xFF;
    data[3] = focus_message.focus_id >> 24;
    data[4] = (focus_message.focus_id >> 16) & 0xFF;
    data[5] = (focus_message.focus_id >> 8) & 0xFF;
    data[6] = focus_message.focus_id & 0xFF;
    data[7] = focus_message.task_name_len;
    if(focus_message.task_name_len > 0)
    {
        memcpy(data + 8, focus_message.task_name, focus_message.task_name_len);
    }
    data_len = focus_message.task_name_len + 8;
}

focus_message_t data_to_focus_message(uint8_t* data)
{
    focus_message_t focus_message;
    focus_message.focus_type = data[0];
    focus_message.focus_time = (data[1] << 8) | data[2];
    focus_message.focus_id = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];  
    focus_message.task_name_len = data[7];
    ESP_LOGI(ESP_NOW, "task_name_len: %d", focus_message.task_name_len);
    if(focus_message.task_name_len > 0)
    {
        memcpy(focus_message.task_name, data + 8, focus_message.task_name_len);
    }
    focus_message.task_name[focus_message.task_name_len] = '\0';
    return focus_message;
}

uint8_t EspNowClient::crc_check(uint8_t *data, int len)
{
    uint8_t crc = 0;
    for(int i = 0; i < len; i++)
    {
        crc += data[i];
    }   
    return crc;
}


void EspNowClient::print_uint8_array(uint8_t *array, size_t length) {
    // 创建一个字符串缓冲区以存储打印内容
    char buffer[512]; // 假设最大长度为 256 字节
    size_t offset = 0;

    for (size_t i = 0; i < length; i++) {
        // 将每个元素格式化为十六进制并追加到缓冲区
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", array[i]);
    }

    // 使用 ESP_LOGI 打印整个数组
    ESP_LOGI("data", "Array: %s\n", buffer);
}

void EspNowClient::send_esp_now_message(uint8_t target_mac[ESP_NOW_ETH_ALEN], uint8_t* data, size_t len, message_type type, bool need_feedback, uint16_t unique_id)
{
    if(len > MAX_EFFECTIVE_DATA_LEN)
    {
        ESP_LOGE(ESP_NOW, "Send espnow message Error occurred: data length is too long");
        return;
    }
    uint8_t pack[ESP_NOW_MAX_DATA_LEN];
    pack[0] = 0xAB;
    pack[1] = ((uint8_t)type << 1) | (need_feedback ? 0x01 : 0x00);
    pack[2] = unique_id >> 8;
    pack[3] = unique_id & 0xFF;
    memcpy(pack + 4, data, len);
    pack[len + 4] = crc_check(data , len);
    pack[len + 5] = 0xEF;
    // 发送广播数据
    esp_err_t err;
    err = esp_now_send(target_mac, pack, len + 6);
    if (err!= ESP_OK) ESP_LOGE(ESP_NOW, "Send espnow message Error occurred: %s", esp_err_to_name(err));
    else 
    {
        // ESP_LOGI(ESP_NOW, "Send espnow message success");
        // print_uint8_array(pack, len + 5);
    }
}

void EspNowClient::send_ack_message(uint8_t _target_mac[ESP_NOW_ETH_ALEN], uint16_t _unique_id)
{
    uint8_t data[2];
    data[0] = _unique_id >> 8;
    data[1] = _unique_id & 0xFF;
    if(m_role == slave_role)
    {
        EspNowClient::Instance()->send_esp_now_message(_target_mac, data, 2, Feedback_ACK, false, _unique_id+1);
    }
    else
    {
        EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, data, 2, Feedback_ACK, false, _unique_id+1);
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
    peer->ifidx = WIFI_IF_STA;
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
    espnow_data_t msg;
    memcpy(msg.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    memcpy(msg.data, data, len);
    msg.data_len = len;
    
    // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(msg.mac_addr), msg.data_len);
    // EspNowClient::Instance()->print_uint8_array(msg.data, msg.data_len);

    if (xQueueSend(EspNowClient::Instance()->recv_message_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(ESP_NOW, "Receive queue fail");
    }
}

void Espclient_recv_packet_task(void* parameters)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(20)); // 避免占用过多CPU
        // 处理接收到的消息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_message_queue) < MAX_RECV_MESSAGE_QUEUE_SIZE)
        {
            espnow_data_t recv_msg;   
            if(xQueueReceive(EspNowClient::Instance()->recv_message_queue, &recv_msg, 0) == pdTRUE)
            {
                // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_msg.mac_addr), recv_msg.data_len);
                // EspNowClient::Instance()->print_uint8_array(recv_msg.data, recv_msg.data_len);
                // 判断收到的包是否有效
                if(recv_msg.data[0] != 0xAB || recv_msg.data[recv_msg.data_len-1] != 0xEF)
                {
                    ESP_LOGE(ESP_NOW, "Receive message header or tail error");
                    continue;
                }
                if(recv_msg.data[recv_msg.data_len-2] != EspNowClient::crc_check(&recv_msg.data[4], recv_msg.data_len-6))
                {
                    ESP_LOGE(ESP_NOW, "Receive message crc error, recieve crc: %d, calc crc: %d", recv_msg.data[recv_msg.data_len-1], EspNowClient::crc_check(&recv_msg.data[4], recv_msg.data_len-6));
                    continue;
                }

                bool need_feedback = recv_msg.data[1] & 0x01;
                uint8_t received_value = (recv_msg.data[1] >> 1);
                message_type m_message_type = static_cast<message_type>(received_value);
                uint16_t unique_id = ((uint16_t)(recv_msg.data[2]) << 8) | recv_msg.data[3];

                //如果需要feedback则返回发送Feedback_ACK消息
                if(need_feedback)
                {
                    EspNowClient::Instance()->send_ack_message(recv_msg.mac_addr, unique_id);
                } 

                // 绑定心跳消息与不参与重复处理，每一条心跳消息都是独一无二的
                if(m_message_type != Bind_Control_Host2Slave)
                {
                    // 检查unique_id是否已存在
                    bool is_duplicate = false;
                    for(auto it = EspNowClient::Instance()->received_unique_ids.begin(); it != EspNowClient::Instance()->received_unique_ids.end(); ++it) {
                        if(*it == unique_id) {
                            is_duplicate = true;
                            // 如果是重复ID，将其从当前位置移除
                            EspNowClient::Instance()->received_unique_ids.erase(it);
                            break;
                        }
                    }
                    
                    // 将ID添加到队列末尾（无论是新的还是重复的）
                    EspNowClient::Instance()->received_unique_ids.push_back(unique_id);
                    
                    // 如果队列大小超过限制，移除最旧的元素（队列前端）
                    if (EspNowClient::Instance()->received_unique_ids.size() > EspNowClient::Instance()->max_received_ids) {
                        EspNowClient::Instance()->received_unique_ids.pop_front();
                    }
                    if(is_duplicate) continue;
                }

                // 解析广播包
                espnow_packet_t packet;
                memcpy(packet.data, &recv_msg.data[4], recv_msg.data_len-6);
                packet.data_len = recv_msg.data_len-6;
                memcpy(packet.mac_addr, recv_msg.mac_addr, ESP_NOW_ETH_ALEN);
                packet.unique_id = unique_id;
                packet.m_message_type = m_message_type;

                //专门给从机激活时的计数
                if(m_message_type == Feedback_ACK)
                {
                    EspNowClient::Instance()->m_recieve_packet_count++;
                }

                if(xQueueSend(EspNowClient::Instance()->recv_packet_queue, &packet, 0) != pdTRUE)
                {
                    ESP_LOGE(ESP_NOW, "Receive packet queue fail");
                }
            }
        }
    }
}

void espnow_update_task(void* parameters)
{
    uint8_t channel = 0;
    const TickType_t channel_switch_delay = pdMS_TO_TICKS(1000);  // 信道切换间隔1秒 ，这里转tick了所以不用转时间
    TickType_t last_switch_time = xTaskGetTickCount();

    while (1)
    {
        // 避免占用过多CPU
        vTaskDelay(pdMS_TO_TICKS(10));
        // 处理接收到的消息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_packet_queue) < MAX_RECV_PACKET_QUEUE_SIZE)
        {
            espnow_packet_t recv_packet;   
            if(xQueueReceive(EspNowClient::Instance()->recv_packet_queue, &recv_packet, 0) == pdTRUE)
            {
                
                // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_packet.mac_addr), recv_packet.data_len);
                // EspNowClient::Instance()->print_uint8_array(recv_packet.data, recv_packet.data_len);
                if(recv_packet.m_message_type == Bind_Control_Host2Slave)
                {
                    Global_data* global_data = get_global_data();
                    global_data->m_host_channel = recv_packet.data[0];
                    memcpy(global_data->m_userName, &recv_packet.data[1], recv_packet.data_len-1);
                    memcpy(global_data->m_host_mac, (uint8_t*)(recv_packet.mac_addr), ESP_NOW_ETH_ALEN);
                    ESP_LOGI(ESP_NOW, "Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d", 
                        global_data->m_userName, 
                        MAC2STR(global_data->m_host_mac), 
                        global_data->m_host_channel);
                    //更新nvs
                    set_nvs_info_set_host_message(global_data->m_host_mac, global_data->m_host_channel, global_data->m_userName);
                    //delay 1s，等待nvs更新
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    EspNowClient::Instance()->is_connect_to_host = true;
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
    recv_message_queue = xQueueCreate(MAX_RECV_MESSAGE_QUEUE_SIZE, sizeof(espnow_data_t));
    if (recv_message_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create message receive queue fail");
        return;
    }

    recv_packet_queue = xQueueCreate(MAX_RECV_PACKET_QUEUE_SIZE, sizeof(espnow_packet_t));
    if (recv_packet_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create packet receive queue fail");
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

    //设置espnow速率
    ESP_ERROR_CHECK(esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_500K));
    xTaskCreate(Espclient_recv_packet_task, "Espclient_recv_packet_task", 4096, NULL, 10, NULL);
}
