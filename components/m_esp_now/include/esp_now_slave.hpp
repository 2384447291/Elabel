#ifndef ESP_NOW_SLAVE_HPP
#define ESP_NOW_SLAVE_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include <algorithm>
#include <stdio.h>
#include <deque>
#include "esp_now_remind.hpp"

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

        TaskHandle_t slave_recieve_update_task_handle = NULL;
        TaskHandle_t slave_send_update_task_handle = NULL; 
        //用来发送的结构
        esp_now_remind remind_host;

        // 从机发送给主机的http消息
        void slave_send_espnow_http_get_todo_list();
        void slave_send_espnow_http_bind_host_request();
        void slave_send_espnow_http_enter_focus_task(focus_message_t focus_message);
        void slave_send_espnow_http_out_focus_task(focus_message_t focus_message);

        // 从机收到主机需要怎么反应
        void slave_respense_espnow_mqtt_get_todo_list(const espnow_packet_t& recv_packet);
        void slave_respense_espnow_mqtt_get_enter_focus(const espnow_packet_t& recv_packet);
        void slave_respense_espnow_mqtt_get_out_focus();
        void slave_respense_espnow_mqtt_get_update_recording(const espnow_packet_t& recv_packet);
};

#endif