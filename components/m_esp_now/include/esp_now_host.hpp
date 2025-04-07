#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"
#include <algorithm>
#include <stdio.h>
#include "freertos/queue.h"
#include "MacAdrees.hpp"

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
    size_t data_len;
    //发送的数据
    uint8_t data[MAX_EFFECTIVE_DATA_LEN];
    //信息类型
    message_type m_message_type;
    //需要发送的从机
    MacAddress Target_slave_mac;
    //在线的从机
    MacAddress Online_slave_mac;
    //是否需要pining
    bool need_pinning = true;
} remind_slave_t;

class EspNowHost {
    public:
        TaskHandle_t host_recieve_update_task_handle = NULL;
        TaskHandle_t host_send_update_task_handle = NULL;
        QueueHandle_t remind_queue = NULL;  // 提醒队列

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
        //上次pin的记录
        MacAddress last_pin_slave_mac;

        
        //当前正在处理的
        remind_slave_t current_remind;
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

        // 添加新的remind_slave到队列
        bool add_remind_to_queue(const remind_slave_t& remind) {
            if (xQueueSend(remind_queue, &remind, 0) != pdTRUE) {
                // ESP_LOGW(ESP_NOW, "Remind queue is full!");
                return false;
            }
            else
            {
                uint16_t queue_length = (uint16_t)uxQueueMessagesWaiting(remind_queue);
                (void)queue_length;
                // ESP_LOGI(ESP_NOW, "Add remind to queue, current queue length: %d.\n", queue_length);
                return true;
            }
        }

        // 获取当前正在处理的remind_slave
        bool get_current_remind(remind_slave_t* remind_slave) {
            if (xQueuePeek(remind_queue, remind_slave, 0) == pdTRUE) {
                return true;
            }
            else
            {
                return false;
            }
        }

        // 完成当前remind_slave的处理
        void finish_current_remind() {
            remind_slave_t temp;
            if (xQueueReceive(remind_queue, &temp, 0) == pdTRUE) {
                // 成功删除队列头部的数据
                ESP_LOGI(ESP_NOW, "Finished processing remind_slave, removed from queue.\n");
            }
        }

        // 发送心跳绑定包
        void send_bind_heartbeat()
        {
            if(send_state != waiting) return;
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
            //只有在waiting状态才能添加从机
            if(send_state != waiting) return;
            if(Bind_slave_mac.insert(slave_mac))
            {
                // 存储从机mac到nvs
                set_nvs_info_set_slave_mac(Bind_slave_mac.count, &(Bind_slave_mac.bytes[0][0]));
                ESP_LOGI(ESP_NOW, "Added new slave " MACSTR " to Bind_slave_mac", MAC2STR(slave_mac));
            }
        }

        // 获取从机反馈
        void get_feedback_from_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            if(send_state != target_sending) return;
            if(current_remind.Online_slave_mac.removeByMac(slave_mac))
            {
                // ESP_LOGI(ESP_NOW, "Removed slave " MACSTR " from online list.\n", MAC2STR(slave_mac));
            }
        }

        // 获取在线的从机
        void get_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            if(send_state != pinning) return;
            //检测是否在target列表中
            if(current_remind.Target_slave_mac.exists(slave_mac))
            {
                //插入到online列表中
                if(current_remind.Online_slave_mac.insert(slave_mac))
                {
                    // ESP_LOGI(ESP_NOW, "Added slave " MACSTR " to online list.\n", MAC2STR(slave_mac));
                }
            }
            else
            {
                ESP_LOGE(ESP_NOW, "Slave " MACSTR " is not in target list", MAC2STR(slave_mac));
            }
        }

        // 打印pinning情况
        void Print_pin_situation()
        {
            if(send_state != pinning) return;
            //记录这次pin的从机
            last_pin_slave_mac = EspNowHost::Instance()->current_remind.Online_slave_mac;
            if (EspNowHost::Instance()->current_remind.Online_slave_mac.count != EspNowHost::Instance()->current_remind.Target_slave_mac.count) 
            {
                ESP_LOGE(ESP_NOW, "Pinning timeout!");
                // 打印在线从机
                ESP_LOGE(ESP_NOW, "Online slaves (%d):", current_remind.Online_slave_mac.count);
                for(size_t i = 0; i < current_remind.Online_slave_mac.count; i++) {
                    ESP_LOGE(ESP_NOW, "  - " MACSTR "", MAC2STR(current_remind.Online_slave_mac.bytes[i]));
                }
                
                // 找出未回复的从机
                MacAddress offline_slaves;
                for(size_t i = 0; i < current_remind.Target_slave_mac.count; i++) 
                {   
                    if(!current_remind.Online_slave_mac.exists(current_remind.Target_slave_mac.bytes[i])) {
                        offline_slaves.insert(current_remind.Target_slave_mac.bytes[i]);
                    }
                }
                // 打印未回复的从机
                ESP_LOGE(ESP_NOW, "Offline slaves (%d):", offline_slaves.count);
                for(size_t i = 0; i < offline_slaves.count; i++) {
                    ESP_LOGE(ESP_NOW, "  - " MACSTR "", MAC2STR(offline_slaves.bytes[i]));
                }
            } 
            else 
            {
                ESP_LOGI(ESP_NOW, "Pinning finished: All slaves online");
            }
        }
};

#endif