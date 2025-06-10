#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "esp_now_host.hpp"
#include "esp_random.h"

void set_is_connect_to_host(bool _is_connect_to_host)
{
    EspNowClient::Instance()->is_connect_to_host = _is_connect_to_host;
}

bool Is_connect_to_host(void)
{
    return EspNowClient::Instance()->is_connect_to_host;
}

//----------------------------------------------打包focus消息----------------------------------------------//
focus_message_t pack_focus_message(uint8_t _focus_type, int _fallTiming, long long _enter_focus_time, int _focus_id, char *_task_name)
{
    focus_message_t focus_message;
    focus_message.focus_type = _focus_type;
    focus_message.fallTiming = _fallTiming;
    focus_message.enter_focus_time = _enter_focus_time;
    focus_message.focus_id = _focus_id;
    focus_message.task_name_len = strlen(_task_name);
    memcpy(focus_message.task_name, _task_name, focus_message.task_name_len);
    return focus_message;
}

void focus_message_to_data(focus_message_t focus_message, uint8_t *data, size_t &data_len)
{
    data[0] = focus_message.focus_type;

    data[1] = focus_message.fallTiming >> 24;
    data[2] = (focus_message.fallTiming >> 16) & 0xFF;
    data[3] = (focus_message.fallTiming >> 8) & 0xFF;
    data[4] = focus_message.fallTiming & 0xFF;

    data[5] = focus_message.focus_id >> 24;
    data[6] = (focus_message.focus_id >> 16) & 0xFF;
    data[7] = (focus_message.focus_id >> 8) & 0xFF;
    data[8] = focus_message.focus_id & 0xFF;

    data[9] = (focus_message.enter_focus_time >> 56) & 0xFF;
    data[10] = (focus_message.enter_focus_time >> 48) & 0xFF;
    data[11] = (focus_message.enter_focus_time >> 40) & 0xFF;
    data[12] = (focus_message.enter_focus_time >> 32) & 0xFF;
    data[13] = (focus_message.enter_focus_time >> 24) & 0xFF;
    data[14] = (focus_message.enter_focus_time >> 16) & 0xFF;
    data[15] = (focus_message.enter_focus_time >> 8) & 0xFF;
    data[16] = focus_message.enter_focus_time & 0xFF;

    data[17] = focus_message.task_name_len;
    if (focus_message.task_name_len > 0)
    {
        memcpy(data + 18, focus_message.task_name, focus_message.task_name_len);
    }
    data_len = focus_message.task_name_len + 18;
}

focus_message_t data_to_focus_message(uint8_t *data)
{
    focus_message_t focus_message;
    focus_message.focus_type = data[0];
    focus_message.fallTiming = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
    focus_message.focus_id = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8];
    focus_message.enter_focus_time = ((int64_t)data[9] << 56) |
                                     ((int64_t)data[10] << 48) |
                                     ((int64_t)data[11] << 40) |
                                     ((int64_t)data[12] << 32) |
                                     ((int64_t)data[13] << 24) |
                                     ((int64_t)data[14] << 16) |
                                     ((int64_t)data[15] << 8) |
                                     ((int64_t)data[16]);
    focus_message.task_name_len = data[17];
    ESP_LOGI(ESP_NOW, "task_name_len: %d", focus_message.task_name_len);
    if (focus_message.task_name_len > 0)
    {
        memcpy(focus_message.task_name, data + 18, focus_message.task_name_len);
    }
    focus_message.task_name[focus_message.task_name_len] = '\0';
    return focus_message;
}
//----------------------------------------------打包focus消息----------------------------------------------//

void espnow_update_task(void *parameters)
{
    uint8_t channel = 0;
    uint8_t actual_wifi_channel = 0;
    wifi_country_t wifi_country;
    esp_wifi_get_country(&wifi_country);
    wifi_country.cc[2] = '\0';
    ESP_LOGI(ESP_NOW, "country: %s, channel: %d, channel num : %d",
             wifi_country.cc, wifi_country.schan, wifi_country.nchan);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(SCAN_CHANNEL_TIME_INTERVAL));
        // 检查是否需要切换信道
        // 只有在未连接到主机时才进行信道切换
        if (!EspNowClient::Instance()->is_connect_to_host)
        {
            channel = channel % wifi_country.nchan + 1;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            esp_wifi_get_channel(&actual_wifi_channel, &wifi_second_channel);
            ESP_LOGI(ESP_NOW, "Set espnow channel to %d", actual_wifi_channel);
        }
    }
}

void EspNowClient::start_find_channel()
{
    if (update_task_handle == NULL)
    {
        xTaskCreate(espnow_update_task, "espnow_update_task", 4096, NULL, 0, &update_task_handle);
        ESP_LOGI(ESP_NOW, "Started channel finding task");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Channel finding task already started");
    }
}

void EspNowClient::stop_find_channel()
{
    if (update_task_handle != NULL)
    {
        vTaskDelete(update_task_handle);
        update_task_handle = NULL;
        ESP_LOGI(ESP_NOW, "Stopped channel finding task");
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Channel finding task already stopped");
    }
}

static esp_err_t Bind_handle(uint8_t *src_addr, void *data,
                             size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    uint8_t *data_ptr = (uint8_t *)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    // 读取数据
    data_ptr++;
    size--;
    if (m_message_type == Host2Slave_Bind_Control_Http)
    {
        // 如果没有连接到主机，则更新主机消息
        if (!EspNowClient::Instance()->is_connect_to_host)
        {
            ESP_LOGI(ESP_NOW, "Receive Bind_Control_Host2Slave message.");
            // 更新连接主机的信息
            Global_data *global_data = get_global_data();
            global_data->m_host_channel = data_ptr[0];

            memcpy(global_data->m_userName, &data_ptr[1], size - 1);
            get_global_data()->m_userName[size - 1] = '\0';

            memcpy(global_data->m_host_mac, (uint8_t *)(src_addr), ESP_NOW_ETH_ALEN);
            ESP_LOGI(ESP_NOW, "Host User name: %s, Host Mac: " MACSTR ", Host Channel: %d",
                     global_data->m_userName,
                     MAC2STR(global_data->m_host_mac),
                     global_data->m_host_channel);
            // 更新nvs
            set_nvs_info_set_host_message(global_data->m_host_mac, global_data->m_host_channel, global_data->m_userName);
            // 保证nvs设置完毕
            vTaskDelay(pdMS_TO_TICKS(1000));
            EspNowClient::Instance()->is_connect_to_host = true;
        }
    }
    else if (m_message_type == Test_Feedback_Host2Slave)
    {
        ESP_LOGI(ESP_NOW, "Receive Test_Feedback_Host2Slave message.");
        EspNowClient::Instance()->test_connecting_send_count = data_ptr[0] | (data_ptr[1] << 8);
        ESP_LOGI(ESP_NOW, "Test Connecting Send Count: %d", EspNowClient::Instance()->test_connecting_send_count);
    }

    return ESP_OK;
}

void EspNowClient::init()
{
    is_connect_to_host = false;
    m_role = default_role;
    update_task_handle = NULL; // 初始化任务句柄为空

    ESP_ERROR_CHECK(espnow_init(&espnow_config));
    // ESP_ERROR_CHECK(esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_500K));
    // 如果没有激活才增加这个回调
    if (get_global_data()->m_is_host == 0)
    {
        ESP_ERROR_CHECK(espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Bind_handle));
    }
}

typedef enum
{
    default_test_connect_process,
    test_connect_process_start,
    test_connect_process_send_packet,
    test_connect_process_stop,
    test_waiting_ack_process,
}Test_connect_process;

Test_connect_process test_connect_process = default_test_connect_process;

TaskHandle_t test_connecting_task_handle = NULL;
bool need_stop_test_connecting = false;
TickType_t start_time = 0;
uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
size_t temp_data_len = MAX_EFFECTIVE_DATA_LEN;

static void test_connecting_task(void *pvParameter)
{
    esp_err_t ret;
    for(int i = 0; i < MAX_EFFECTIVE_DATA_LEN; i++)
    {
        temp_data[i] = esp_random() & 0xFF;
    }
    while(!need_stop_test_connecting)
    {
        switch(test_connect_process)
        {
            case default_test_connect_process:
            break;
            case test_connect_process_start:
            {
                do{
                    ret = EspNowClient::Instance()->send_message(temp_data, temp_data_len, Test_Start_Request_Slave2Host, get_global_data()->m_host_mac);
                }while(ret!=ESP_OK);
                test_connect_process = test_connect_process_send_packet;
            }
            break;
            case test_connect_process_send_packet:
            {
                start_time = xTaskGetTickCount();
                do{
                    ret = EspNowClient::Instance()->send_test_message(temp_data, temp_data_len, get_global_data()->m_host_mac);
                }while(xTaskGetTickCount() - start_time < pdMS_TO_TICKS(2000));
                EspNowClient::Instance()->test_connecting_send_count = -1;
                test_connect_process = test_connect_process_stop;
            }
            break;
            case test_connect_process_stop:
            {
                do{
                    ret = EspNowClient::Instance()->send_message(temp_data, temp_data_len, Test_Stop_Request_Slave2Host, get_global_data()->m_host_mac);
                }while(ret!=ESP_OK);
                test_connect_process = test_waiting_ack_process;
            }
            break;
            case test_waiting_ack_process:
            {
                while(EspNowClient::Instance()->test_connecting_send_count == -1)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                test_connect_process = test_connect_process_start;
            }
            break;
        }
    }
    test_connecting_task_handle = NULL;
    vTaskDelete(NULL);
}

void EspNowClient::start_test_connecting_task(bool need_add_new_peer)
{
    if(test_connecting_task_handle == NULL)
    {
        //添加主机为peer
        if(need_add_new_peer) espnow_add_peer(get_global_data()->m_host_mac, NULL);
        need_stop_test_connecting = false;
        test_connect_process = test_connect_process_start;
        xTaskCreate(test_connecting_task, "test_connecting_task", 4096, NULL, 10, &test_connecting_task_handle);
    }
}

void EspNowClient::stop_test_connecting_task(bool need_add_new_peer)
{
    if(test_connecting_task_handle != NULL)
    {
        TickType_t stop_tick = xTaskGetTickCount();
        //等待测试历程运行一个循环再删除或者4s超时
        while(test_connect_process != test_waiting_ack_process && xTaskGetTickCount() - stop_tick < pdMS_TO_TICKS(4000))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        if(need_add_new_peer) espnow_del_peer(get_global_data()->m_host_mac);
        need_stop_test_connecting = true;
    }
}
