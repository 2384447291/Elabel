#ifndef ESP_NOW_SLAVE_HPP
#define ESP_NOW_SLAVE_HPP

#include "esp_now_client.hpp"

typedef struct {
    //该次发送的包的唯一id
    uint16_t unique_id = 0;
    //发送的数据长度
    int data_len;
    //发送的数据
    uint8_t data[MAX_EFFECTIVE_DATA_LEN];
    //发送的消息类型
    message_type m_message_type;
} remind_host_t;

class EspNowSlave {
    private:
        TaskHandle_t slave_recieve_update_task_handle = NULL;
        TaskHandle_t slave_send_update_task_handle = NULL;
    public:   
        uint8_t host_mac[ESP_NOW_ETH_ALEN];
        uint8_t host_channel;
        bool need_send_data = false;
        remind_host_t remind_host;

        char username[100];
        void init();
        void deinit();

        // 上一次收到包的时间
        TickType_t last_recv_heart_time = 0;  

        void send_bind_message();
        void send_out_focus_message(int task_id);
        void send_enter_focus_message(int task_id, int fall_time);
        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }
};

#endif