#ifndef ESP_NOW_HOST_HPP
#define ESP_NOW_HOST_HPP

#include "esp_now_client.hpp"
#include "global_nvs.h"

enum host_send_state
{
    waiting, //没有发送需求，发送心跳即可
    pinning, //需要唤醒设备
    broadcasting, //广播消息，不许要确定接收者
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
    //这次发送还未收到反馈的从机
    uint8_t slave_num_not_feedback = 0;
    //这次发送还未收到反馈的从机
    uint8_t slave_mac_not_feedback[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
} remind_slave_t;

class EspNowHost {
    private:
        TaskHandle_t host_recieve_update_task_handle = NULL;
        TaskHandle_t host_send_update_task_handle = NULL;
    public:
        //绑定的从机的数目和地址
        uint8_t slave_mac[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
        uint8_t slave_num;
        //在线的从机
        uint8_t online_slave_mac[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
        uint8_t online_slave_num;

        remind_slave_t remind_slave;
        host_send_state send_state = waiting;

        //广播绑定消息
        void Broadcast_bind_message();

        //查询在线的从机
        void pin_slave();
        // 开始pinning的时间
        TickType_t pinning_start_time = 0;  
        // 2秒超时
        const TickType_t PINNING_TIMEOUT = pdMS_TO_TICKS(2000);  
        // 添加在线从机
        void add_online_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN]);
        // pin消息的唯一id
        uint16_t pin_message_unique_id = 0;

        void empty_remind_slave()
        {
            memset(remind_slave.data, 0, sizeof(remind_slave.data));
            remind_slave.data_len = 0;
            remind_slave.m_message_type = default_message_type;
            remind_slave.unique_id = 0;
            remind_slave.slave_num_not_feedback = 0;
            memset(remind_slave.slave_mac_not_feedback, 0, sizeof(remind_slave.slave_mac_not_feedback));
        }

        void set_SlaveList2NVS()
        {
            // 存储从机mac到nvs
            uint8_t mac_bytes[MAX_SLAVE_NUM * 6];  // 每个MAC地址6个字节
            memset(mac_bytes, 0, sizeof(mac_bytes));
            for(int i = 0; i < slave_num; i++) {
                memcpy(&mac_bytes[i * 6], slave_mac[i], 6);
            }
            // 存储到NVS
            set_nvs_info_set_slave_mac(slave_num, mac_bytes);
        }

        void get_feedback_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            //先查找改slave_mac是否在slave_num_not_feedback中,如果在就从数组中删掉这个mac
            for(int i = 0; i < remind_slave.slave_num_not_feedback; i++)
            {
                if(memcmp(slave_mac, remind_slave.slave_mac_not_feedback[i], ESP_NOW_ETH_ALEN) == 0)
                {
                    ESP_LOGI(ESP_NOW, "Get feedback slave success MAC: " MACSTR "", MAC2STR(slave_mac));
                    //从数组中删掉这个mac
                    for(int j = i; j < remind_slave.slave_num_not_feedback - 1; j++)
                    {
                        for(int k = 0; k < ESP_NOW_ETH_ALEN; k++)
                        {
                            remind_slave.slave_mac_not_feedback[j][k] = remind_slave.slave_mac_not_feedback[j + 1][k];
                        }
                    }
                    remind_slave.slave_num_not_feedback--;
                }
            }

            //如果slave_num_not_feedback为0,则完成这次target_sending
            if(remind_slave.slave_num_not_feedback == 0)
            {
                send_state = waiting;
            }
        }

        void add_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void del_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void init();
        void deinit();


        void send_broadcast_message();
        void send_update_task_list();
        void send_enter_focus(TodoItem _todo_item);
        void send_out_focus();

        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }
        EspNowHost() {}
};

#endif