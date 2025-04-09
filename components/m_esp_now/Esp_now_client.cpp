#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "esp_now_host.hpp"

void set_is_connect_to_host(bool _is_connect_to_host)
{
    EspNowClient::Instance()->is_connect_to_host = _is_connect_to_host;
}

bool Is_connect_to_host(void)
{
    return EspNowClient::Instance()->is_connect_to_host;
}

//----------------------------------------------打包focus消息----------------------------------------------//
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
//----------------------------------------------打包focus消息----------------------------------------------//

void espnow_update_task(void* parameters)
{
    uint8_t channel = 0;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_packet.mac_addr), recv_packet.data_len);
        // EspNowClient::Instance()->print_uint8_array(recv_packet.data, recv_packet.data_len);
        // 检查是否需要切换信道
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

static esp_err_t Bind_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    // ESP_PARAM_CHECK(src_addr);
    // ESP_PARAM_CHECK(data);
    // ESP_PARAM_CHECK(size);
    // ESP_PARAM_CHECK(rx_ctrl);

    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;
    if(m_message_type == Bind_Control_Host2Slave)
    {
        Global_data* global_data = get_global_data();
        global_data->m_host_channel = data_ptr[0];
        memcpy(global_data->m_userName, &data_ptr[1], size-1);
        memcpy(global_data->m_host_mac, (uint8_t*)(src_addr), ESP_NOW_ETH_ALEN);
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

    return ESP_OK;
}

void EspNowClient::init(){
    is_connect_to_host = false;
    m_role = default_role;
    update_task_handle = NULL;  // 初始化任务句柄为空
    
    espnow_config_t espnow_config =  {  
    .pmk = {'E', 'S', 'P', '_', 'N', 'O', 'W', 0}, 
    .forward_enable = 0,  //是否开启转发
    .forward_switch_channel = 0,  //是否开启信道切换
    .sec_enable = 0,  //是否开启加密
    .reserved1 = 0,    //保留
    .qsize = 32,  //发送任务队列的大小
    .send_retry_num = 10,  //感觉像费案，没有任何地方用
    .send_max_timeout = ESPNOW_SEND_MAX_TIMEOUT,  
    //send_max_timeout只用来等待发送队列，一般wait_ticks 要 大于 send_max_timeout
    //wait_ticks 是整个 espnow_send() 调用的预算，包括了这段时间
    .receive_enable = { 
        .ack           = 1,  //是否开启ack
        .forward       = 0,  //是否开启转发
        .group         = 0,  //是否开启组播
        .provisoning   = 0,  //是否开启预设
        .control_bind  = 0,  //是否开启控制绑定
        .control_data  = 0,  //是否开启控制数据
        .ota_status    = 0,  //是否开启ota状态
        .ota_data      = 0, 
        .debug_log     = 0, 
        .debug_command = 0, 
        .data          = 0, 
        .sec_status    = 0, 
        .sec           = 0, 
        .sec_data      = 0, 
        .reserved2     = 0, 
        }, 
    };

    espnow_init(&espnow_config);
    // ESP_ERROR_CHECK(esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_500K));
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Bind_handle);
}
