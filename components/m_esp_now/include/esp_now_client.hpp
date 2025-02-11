#ifndef ESP_NOW_CLIENT_HPP
#define ESP_NOW_CLIENT_HPP

#include "esp_now.h"
#include "esp_log.h"
#include "network.h"
#include "global_message.h"
#include "freertos/queue.h"

#define ESP_NOW "ESPNOW"
#define MAX_DATA_LEN 100
#define MAX_RECV_QUEUE_SIZE 10

#define MAX_SLAVE_NUM 6

// 定义ESP-NOW消息结构体
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN]; // 发送者的mac地址
    uint8_t data[MAX_DATA_LEN]; // 数据
    int data_len;
} espnow_message_t;

typedef enum {
    Bind_broadcast_message = 25,
    Bind_feedback_message = 26,
} message_type;

typedef enum {
    default_role,
    host_role,
    slave_role,
} esp_now_role;

class EspNowClient{
    private:
        TaskHandle_t update_task_handle = NULL;
    public:
        void init();
        esp_now_role m_role = default_role;
        void Addpeer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN]);
        void Delpeer(uint8_t peer_mac[ESP_NOW_ETH_ALEN]);
        void ModifyPeer(uint8_t peer_mac[ESP_NOW_ETH_ALEN], uint8_t peer_chaneel);
        void send_esp_now_message(uint8_t target_mac[8], uint8_t* data, size_t len);
        //更新函数
        static void espnow_update_task(void* parameters);
        void start_find_channel();
        void stop_find_channel();
        QueueHandle_t recv_queue;  // 接收消息队列

        bool is_connect_to_host = false;

        static EspNowClient* Instance() {
            static EspNowClient instance;
            return &instance;
        }

        //打印uint8_t数组
        void print_uint8_array(uint8_t *array, size_t length);
};


bool Is_connect_to_host();

void set_is_connect_to_host(bool _is_connect_to_host);


class EspNowHost {
    private:
        EspNowClient* client;
        TaskHandle_t host_recieve_update_task_handle = NULL;
    public:
        uint8_t slave_mac[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
        uint8_t slave_num;

        void add_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void del_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void init();
        void deinit();
        void send_broadcast_message();

        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }
        EspNowHost() {
            client = EspNowClient::Instance();
        }
};

class EspNowSlave {
    private:
        EspNowClient* client;
        TaskHandle_t slave_recieve_update_task_handle = NULL;
    public:
        uint8_t host_mac[ESP_NOW_ETH_ALEN];
        uint8_t host_channel;
        char username[100];
        void init();
        void deinit();

        void send_feedback_message();

        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }
        EspNowSlave() {
            client = EspNowClient::Instance();
        }
};

#endif