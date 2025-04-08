#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include <algorithm>
#include <stdio.h>
#include <deque>
#include "esp_now_remind.hpp"

#define MAX_REMIND_COUNT 10

class EspNowHost {
    public:
        TaskHandle_t host_recieve_update_task_handle = NULL;
        TaskHandle_t host_send_update_task_handle = NULL;
        std::deque<esp_now_remind_host> remind_queue;  // 使用std::deque

        //是否正在处理任务
        bool is_available = false;

        //收到http的相应发送的mqtt更新任务列表
        void Mqtt_update_task_list(uint8_t target_mac[ESP_NOW_ETH_ALEN] = BROADCAST_MAC, bool need_broadcast = false);

        //mqtt更新任务列表
        void Mqtt_enter_focus(focus_message_t focus_message);
        void Mqtt_out_focus();
        void Start_Mqtt_update_recording();
        void Stop_Mqtt_update_recording();

        //http的响应函数
        void http_response_enter_focus(const espnow_packet_t& recv_packet);
        void http_response_out_focus(const espnow_packet_t& recv_packet);

        //绑定的从机的数目和地址
        MacAddress Bind_slave_mac;

        void init();
        void deinit();

        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }
        EspNowHost() {}

        // 添加新的remind_slave到队列
        bool add_remind_to_queue(const esp_now_remind_host& remind) {
            if (remind_queue.size() >= MAX_REMIND_COUNT) {
                ESP_LOGW(ESP_NOW, "Remind queue is full!");
                return false;
            }
            remind_queue.push_back(remind);
            return true;
        }

        // 获取当前正在处理的remind_slave
        bool get_current_remind(esp_now_remind_host* remind_slave) {
            if (remind_queue.empty()) {
                return false;
            }
            *remind_slave = remind_queue.front();
            return true;
        }

        // 完成当前remind_slave的处理
        void finish_current_remind(uint16_t unique_id) {
            auto it = std::find_if(remind_queue.begin(), remind_queue.end(),
                [unique_id](const esp_now_remind_host& remind) {
                    return remind.unique_id == unique_id;
                });
            if (it != remind_queue.end()) {
                remind_queue.erase(it);
            }
        }


        // 发送心跳绑定包
        void send_bind_heartbeat()
        {
            uint8_t wifi_channel = 0;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

            uint8_t username_len = strlen(get_global_data()->m_userName);
            uint8_t total_len = 1 + username_len;
            uint8_t broadcast_data[total_len];
            
            broadcast_data[0] = wifi_channel;
            memcpy(&broadcast_data[1], get_global_data()->m_userName, username_len);

            uint16_t unique_id = esp_random() & 0xFFFF;
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, broadcast_data, total_len, Bind_Control_Host2Slave, false, unique_id);
        }

        // 添加新的从机到Bind_slave_mac，并存到nvs中
        void Add_new_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            if(Bind_slave_mac.insert(slave_mac))
            {
                // 存储从机mac到nvs
                set_nvs_info_set_slave_mac(Bind_slave_mac.count, &(Bind_slave_mac.bytes[0][0]));
                ESP_LOGI(ESP_NOW, "Added new slave " MACSTR " to Bind_slave_mac", MAC2STR(slave_mac));
            }
        }
};

#endif