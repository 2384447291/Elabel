#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include "MacAdrees.hpp"
#include "espnow.h"

#define HEART_BEAT_TIME_MSECS 1000

class EspNowHost {
    public:
        bool is_sending_message = false;
        void init();
        void deinit();
        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }

        TaskHandle_t host_send_update_task_handle = NULL;

        //收到http的相应发送的mqtt更新任务列表
        void Mqtt_get_device_info(const uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void Mqtt_get_time(const uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void Mqtt_send_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        //mqtt更新任务列表
        void Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN] = ESPNOW_ADDR_BROADCAST);
        void Mqtt_enter_focus(focus_message_t focus_message);
        void Mqtt_out_focus();

        //http的响应函数
        void http_response_enter_focus(uint8_t* data, size_t size);
        void http_response_out_focus(uint8_t* data, size_t size);

        //差不多1s，如果没有收到就算了
        esp_err_t send_message_ack(uint8_t* data, size_t size, message_type m_message_type, const espnow_addr_t dest_addr)
        {
            is_sending_message = true;
            uint8_t packet_data[size+1];
            packet_data[0] = m_message_type;
            memcpy(&packet_data[1], data, size);
            esp_err_t ret = ESP_FAIL;
            uint8_t count = 0;
            do{
                ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, packet_data,
                        size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
                count++;
            }while(ret != ESP_OK && count < HOST_ASK_SLAVE_TIME);
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

        // 发送心跳绑定包
        esp_err_t send_bind_heartbeat()
        {
            if(is_sending_message) return ESP_FAIL;
            uint8_t wifi_channel = 0;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

            uint8_t username_len = strlen(get_global_data()->m_userName);
            uint8_t total_len = 2 + username_len;
            uint8_t broadcast_data[total_len];
            
            broadcast_data[0] = Host2Slave_Bind_Control_Http;
            broadcast_data[1] = wifi_channel;
            memcpy(&broadcast_data[2], get_global_data()->m_userName, username_len);
            
            // 发送心跳绑定包,没有ack的包无所谓delaytime，包成功的
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, broadcast_data,
                      total_len, &EspNowClient::Instance()->broadcast_head, ESPNOW_SEND_MAX_TIMEOUT);
            return ret;
        }


        // 添加新的从机到Bind_slave_mac，并存到nvs中
        void Add_new_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            //添加到global_data，如果添加成功则添加到nvs
            if(insert_slave(slave_mac))
            {
                // 存储从机mac到nvs
                uint8_t data[get_global_data()->m_slave_num * 6];
                for(int i = 0; i < get_global_data()->m_slave_num; i++)
                {
                    memcpy(&data[i * 6], get_global_data()->m_slave_info[i].mac, 6);
                }
                set_nvs_info_set_slave_mac(get_global_data()->m_slave_num, data);
                ESP_LOGI(ESP_NOW, "Added new slave " MACSTR " to Bind_slave_mac", MAC2STR(slave_mac));
                espnow_add_peer(slave_mac, NULL);
            }
        }
};

#endif