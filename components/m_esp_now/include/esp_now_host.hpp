#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include <algorithm>
#include <stdio.h>
#include "freertos/queue.h"
#include <vector>
#define PINNING_TIMEOUT_MS 2000

enum host_send_state
{
    waiting, //没有发送需求，发送心跳即可
    pinning, //需要唤醒设备
    target_sending, //发送消息，需要确定接收者
};

typedef struct {
    //该次发送的包的唯一id
    uint16_t unique_id = 0;
    //发送的数据长度
    int data_len;
    //发送的数据
    uint8_t data[MAX_EFFECTIVE_DATA_LEN];
    //信息类型
    message_type m_message_type;
    //需要发送的从机
    std::vector<MacAddress> Target_slave_mac;
    //在线的从机
    std::vector<MacAddress> Online_slave_mac;
} remind_slave_t;

class EspNowHost {
    private:
        TaskHandle_t host_recieve_update_task_handle = NULL;
        TaskHandle_t host_send_update_task_handle = NULL;
        QueueHandle_t remind_queue = NULL;  // 提醒队列
    public:
        //绑定的从机的数目和地址
        std::vector<MacAddress> Bind_slave_mac;
        
        remind_slave_t remind_slave;
        //发送状态
        host_send_state send_state = waiting;
        // 是否正在执行pin操作
        bool is_pinning = false;

        // 开始pinning的时间
        TickType_t pinning_start_time = 0;  
        // pin消息的唯一id
        uint16_t pin_message_unique_id = 0;

        void init();
        void deinit();

        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }
        EspNowHost() {}

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
            // 检查是否已经存在该从机
            for(const auto& mac : Bind_slave_mac) {
                if(memcmp(slave_mac, mac.bytes, ESP_NOW_ETH_ALEN) == 0) {
                    // ESP_LOGI(ESP_NOW, "Slave " MACSTR " already exists in Bind_slave_mac", MAC2STR(slave_mac));
                    return;
                }
            }

            // 检查是否达到最大从机数量
            if(Bind_slave_mac.size() >= MAX_SLAVE_NUM) {
                ESP_LOGW(ESP_NOW, "Cannot add more slaves, reached maximum limit of %d", MAX_SLAVE_NUM);
                return;
            }

            // 添加新的从机MAC地址
            MacAddress new_mac;
            memcpy(new_mac.bytes, slave_mac, ESP_NOW_ETH_ALEN);
            Bind_slave_mac.push_back(new_mac);

            // 存储从机mac到nvs
            uint8_t mac_bytes[MAX_SLAVE_NUM * 6];  // 每个MAC地址6个字节
            memset(mac_bytes, 0, sizeof(mac_bytes));
            for(int i = 0; i < Bind_slave_mac.size(); i++) {
                memcpy(&mac_bytes[i * 6], Bind_slave_mac[i].bytes, 6);
            }
            // 存储到NVS
            set_nvs_info_set_slave_mac(Bind_slave_mac.size(), mac_bytes);
            
            ESP_LOGI(ESP_NOW, "Added new slave " MACSTR " to Bind_slave_mac", MAC2STR(slave_mac));
        }

        // 添加新的remind_slave到队列
        bool add_remind_to_queue(const remind_slave_t& remind) {
            if (xQueueSend(remind_queue, &remind, 0) != pdTRUE) {
                ESP_LOGW(ESP_NOW, "Remind queue is full!");
                return false;
            }
            else
            {
                uint16_t queue_length = (uint16_t)uxQueueMessagesWaiting(remind_queue);
                (void)queue_length;
                ESP_LOGI(ESP_NOW, "Add remind to queue, current queue length: %d", queue_length);
                return true;
            }
        }

        // 获取当前正在处理的remind_slave
        remind_slave_t* get_current_remind() {
            remind_slave_t* current_remind = nullptr;
            if (xQueuePeek(remind_queue, &current_remind, 0) == pdTRUE) {
                return current_remind;
            }
            return nullptr;
        }

        // 完成当前remind_slave的处理
        void finish_current_remind() {
            remind_slave_t temp;
            if (xQueueReceive(remind_queue, &temp, 0) == pdTRUE) {
                // 成功删除队列头部的数据
                ESP_LOGI(ESP_NOW, "Finished processing remind_slave, removed from queue");
            }
        }

        // 清空remind_slave
        void empty_remind_slave()
        {
            memset(remind_slave.data, 0, sizeof(remind_slave.data));
            remind_slave.data_len = 0;
            remind_slave.m_message_type = default_message_type;
            remind_slave.unique_id = 0;
            remind_slave.Target_slave_mac.clear();
            remind_slave.Online_slave_mac.clear();
        }

        // 获取从机反馈
        void get_feedback_from_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            remind_slave_t* current_remind = get_current_remind();
            if (current_remind == nullptr) return;

            // 使用迭代器查找并删除匹配的MAC地址
            auto it = std::find_if(current_remind->Online_slave_mac.begin(), 
                                 current_remind->Online_slave_mac.end(),
                                 [&slave_mac](const MacAddress& mac) {
                                     return memcmp(mac.bytes, slave_mac, ESP_NOW_ETH_ALEN) == 0;
                                 });

            if (it != current_remind->Online_slave_mac.end()) {
                ESP_LOGI(ESP_NOW, "Get feedback from slave " MACSTR "", MAC2STR(slave_mac));
                current_remind->Online_slave_mac.erase(it);
            }
        }

        // 获取在线的从机
        void get_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            remind_slave_t* current_remind = get_current_remind();
            if (current_remind == nullptr) return;

            // 检查是否在target列表中
            bool is_target = false;
            for(const auto& target_mac : current_remind->Target_slave_mac) {
                if(memcmp(slave_mac, target_mac.bytes, ESP_NOW_ETH_ALEN) == 0) {
                    is_target = true;
                    break;
                }
            }

            if (!is_target) {
                ESP_LOGI(ESP_NOW, "Slave " MACSTR " is not in target list", MAC2STR(slave_mac));
                return;
            }

            // 检查是否已经在Online列表中
            bool already_online = false;
            for(const auto& online_mac : current_remind->Online_slave_mac) {
                if(memcmp(slave_mac, online_mac.bytes, ESP_NOW_ETH_ALEN) == 0) {
                    already_online = true;
                    break;
                }
            }

            if (already_online) {
                ESP_LOGI(ESP_NOW, "Slave " MACSTR " is already online", MAC2STR(slave_mac));
                return;
            }

            // 添加到Online列表
            MacAddress new_mac;
            memcpy(new_mac.bytes, slave_mac, ESP_NOW_ETH_ALEN);
            current_remind->Online_slave_mac.push_back(new_mac);
            ESP_LOGI(ESP_NOW, "Add slave " MACSTR " to online list", MAC2STR(slave_mac));
        }

        // 打印pinning情况
        void Print_pin_situation()
        {
            remind_slave_t* current_remind = get_current_remind();
            if (current_remind == nullptr) return;

            if (xTaskGetTickCount() - EspNowHost::Instance()->pinning_start_time >= pdMS_TO_TICKS(PINNING_TIMEOUT_MS)) 
            {
                ESP_LOGI(ESP_NOW, "Pinning timeout!");
                // 打印在线从机
                ESP_LOGI(ESP_NOW, "Online slaves (%d):", current_remind->Online_slave_mac.size());
                for(const auto& mac : current_remind->Online_slave_mac) {
                    (void)mac;
                    ESP_LOGI(ESP_NOW, "  - " MACSTR "", MAC2STR(mac.bytes));
                }
                
                // 找出未回复的从机
                std::vector<MacAddress> offline_slaves;
                for(const auto& target_mac : current_remind->Target_slave_mac) {
                    bool found = false;
                    for(const auto& online_mac : current_remind->Online_slave_mac) {
                        if(memcmp(target_mac.bytes, online_mac.bytes, ESP_NOW_ETH_ALEN) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if(!found) {
                        offline_slaves.push_back(target_mac);
                    }
                }
                // 打印未回复的从机
                ESP_LOGI(ESP_NOW, "Offline slaves (%d):", offline_slaves.size());
                for(const auto& mac : offline_slaves) {
                    (void)mac;
                    ESP_LOGI(ESP_NOW, "  - " MACSTR "", MAC2STR(mac.bytes));
                }
            } 
            else 
            {
                ESP_LOGI(ESP_NOW, "Pinning finished: All slaves online");
            }
        }
};

#endif