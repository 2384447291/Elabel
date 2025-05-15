#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include "MacAdrees.hpp"
#include "espnow.h"

#define HEART_BEAT_TIME_MSECS 2000

class EspNowHost {
    public:
        //绑定的从机的数目和地址
        MacAddress Bind_slave_mac;
        bool is_sending_message = false;
        void init();
        void deinit();
        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }

        //收到http的相应发送的mqtt更新任务列表
        void Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN] = ESPNOW_ADDR_BROADCAST);

        //mqtt更新任务列表
        void Mqtt_enter_focus(focus_message_t focus_message);
        void Mqtt_out_focus();

        //http的响应函数
        void http_response_enter_focus(uint8_t* data, size_t size);
        void http_response_out_focus(uint8_t* data, size_t size);

        //差不多2s，如果没有收到就算了
        esp_err_t send_message_ack(uint8_t* data, size_t size, message_type m_message_type, const espnow_addr_t dest_addr)
        {
            is_sending_message = true;
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, packet_data,
                      size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
            is_sending_message = false;
            return ret;
        }

        //不需要ack但是要发送出去,超时半s
        esp_err_t send_message_no_ack(uint8_t* data, size_t size, message_type m_message_type, const espnow_addr_t dest_addr)
        {
            is_sending_message = true;
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, packet_data,
                      size+1, &EspNowClient::Instance()->test_send_head, ESPNOW_SEND_MAX_TIMEOUT);
            is_sending_message = false;
            return ret;
        }


        // 添加新的从机到Bind_slave_mac，并存到nvs中
        void Add_new_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            if(Bind_slave_mac.insert(slave_mac))
            {
                // 存储从机mac到nvs
                uint8_t data[Bind_slave_mac.count * 6];
                for(int i = 0; i < Bind_slave_mac.count; i++)
                {
                    memcpy(&data[i * 6], Bind_slave_mac.slaves[i].mac, 6);
                }
                set_nvs_info_set_slave_mac(Bind_slave_mac.count, data);
                ESP_LOGI(ESP_NOW, "Added new slave " MACSTR " to Bind_slave_mac", MAC2STR(slave_mac));
                espnow_add_peer(slave_mac, NULL);
            }
        }
};

#endif