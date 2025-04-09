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

        TaskHandle_t host_send_update_task_handle = NULL;

        //收到http的相应发送的mqtt更新任务列表
        void Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN] = ESPNOW_ADDR_BROADCAST);

        //mqtt更新任务列表
        void Mqtt_enter_focus(focus_message_t focus_message);
        void Mqtt_out_focus();
        void Start_Mqtt_update_recording();
        void Stop_Mqtt_update_recording();

        //http的响应函数
        void http_response_enter_focus(uint8_t* data, size_t size);
        void http_response_out_focus(uint8_t* data, size_t size);

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
            
            broadcast_data[0] = Bind_Control_Host2Slave;
            broadcast_data[1] = wifi_channel;
            memcpy(&broadcast_data[2], get_global_data()->m_userName, username_len);
            
            // 发送心跳绑定包,没有ack的包无所谓delaytime，包成功的
            esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, broadcast_data,
                      total_len, &EspNowClient::Instance()->broadcast_head, portMAX_DELAY);
            // ESP_ERROR_CONTINUE(ret != ESP_OK, "<%s> espnow_send", esp_err_to_name(ret));
            return ret;
        }

        esp_err_t send_message(uint8_t* data, size_t size, message_type m_message_type, const espnow_addr_t dest_addr)
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