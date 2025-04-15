#ifndef ESP_NOW_SLAVE_HPP
#define ESP_NOW_SLAVE_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"

class EspNowSlave {
    public:   
        uint8_t host_mac[ESP_NOW_ETH_ALEN];
        uint8_t host_channel;
        char username[100];
        bool is_host_connected = false;
        // 上一次收到包的时间
        TickType_t last_recv_heart_time = 0;  

        void init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100]);
        void deinit();
        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }

        TaskHandle_t slave_send_update_task_handle = NULL; 

        // 从机发送给主机的http消息
        void slave_send_espnow_http_get_todo_list();
        void slave_send_espnow_http_bind_host_request();
        void slave_send_espnow_http_enter_focus_task(focus_message_t focus_message);
        void slave_send_espnow_http_out_focus_task(focus_message_t focus_message);

        // 从机收到主机需要怎么反应
        void slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size);
        void slave_respense_espnow_mqtt_get_out_focus();

        esp_err_t send_message(uint8_t* data, size_t size, message_type m_message_type)
        {
            if(!is_host_connected)
            {
                ESP_LOGI(ESP_NOW, "Slave not connected to host");
                return ESP_FAIL;
            }
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, host_mac, packet_data,
                      size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
            return ret;
        }
};

#endif