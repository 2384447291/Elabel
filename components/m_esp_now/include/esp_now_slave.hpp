#ifndef ESP_NOW_SLAVE_HPP
#define ESP_NOW_SLAVE_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"

class EspNowSlave {
    public:   
        uint8_t host_mac[ESP_NOW_ETH_ALEN];
        uint8_t host_channel;
        char username[100];
        // 上一次收到包的时间
        TickType_t last_recv_heart_time = 0;  
        uint8_t sleep_sync_flag = 3;//0表示需要同步，1表示focus同步，2表示时间同步, 3表示无需同步
        bool waiting_host_feedback = false;

        void init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100]);
        void deinit();
        void resume_espnow();
        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }

        // 从机发送给主机的http消息
        esp_err_t slave_send_espnow_http_get_todo_list();
        esp_err_t slave_send_espnow_http_get_device_info();
        esp_err_t slave_send_espnow_http_get_time();
        esp_err_t slave_send_espnow_http_enter_focus_task(focus_message_t focus_message);
        esp_err_t slave_send_espnow_http_out_focus_task(focus_message_t focus_message);
        esp_err_t slave_send_espnow_http_sleep_request();
        esp_err_t slave_send_espnow_http_wakeup_request();
        esp_err_t slave_send_espnow_http_synchronous_request();
        esp_err_t slave_send_espnow_http_get_wifi_info();
        esp_err_t slave_send_espnow_http_get_user_token();
        esp_err_t slave_send_espnow_http_unbind_device();
        esp_err_t slave_send_espnow_http_send_power_message(int power_state);
        esp_err_t slave_send_espnow_http_delete_task(int task_id);

        // 从机收到主机需要怎么反应
        void slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_device_info(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_time(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_out_focus();
        void slave_respense_espnow_mqtt_send_task_list(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_wifi_info(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_user_token(uint8_t* data, size_t size);

        // 必须收到ack，爆发1s共10次，尝试4次
        esp_err_t send_message(uint8_t* data, size_t size, message_type m_message_type)
        {
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = ESP_FAIL;
            uint8_t count = 0;
            do{
                ret = espnow_send(ESPNOW_DATA_TYPE_DATA, host_mac, packet_data,
                        size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
                count++;
            }while(ret != ESP_OK && count < SLAVE_ASK_HOST_TIME);
            return ret;
        }

        // 必须收到ack，爆发1s共10次，尝试1次
        esp_err_t send_message_once(uint8_t* data, size_t size, message_type m_message_type)
        {
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = ESP_FAIL;
            uint8_t count = 0;
            do{
                ret = espnow_send(ESPNOW_DATA_TYPE_DATA, host_mac, packet_data,
                        size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
                count++;
            }while(ret != ESP_OK && count < 1);
            return ret;
        }
};

#endif