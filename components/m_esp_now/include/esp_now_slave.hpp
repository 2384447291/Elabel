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

        void init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100]);
        void deinit();
        void suspend_espnow();
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

        // 从机收到主机需要怎么反应
        void slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_device_info(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_time(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_out_focus();
        void slave_respense_espnow_mqtt_send_task_list(uint8_t* data, size_t size);

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
};

#endif