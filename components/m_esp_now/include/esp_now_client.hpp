#ifndef ESP_NOW_CLIENT_HPP
#define ESP_NOW_CLIENT_HPP

#include "esp_now.h"
#include "esp_log.h"
#include "network.h"
#include "global_message.h"
#include "freertos/queue.h"
#include <set>

#define ESP_NOW "ESPNOW"
#define MAX_RECV_MESSAGE_QUEUE_SIZE 32
#define MAX_RECV_PACKET_QUEUE_SIZE 32
#define PACKET_HEAD 0xAB
#define PACKET_TAIL 0xEF
//数据最大长度等于减去包头包尾crc和唯一id和标识符
#define MAX_EFFECTIVE_DATA_LEN (ESP_NOW_MAX_DATA_LEN - 1 - 1 - 2 - 1 - 1)

//1s25个包
#define Esp_Now_Send_Interval 20
extern uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN];

// 消息类型用7位标识
// 这里采用优化的结构，http只会从从机发送到主机，主机会根据从机的http请求，发送对应的mqtt信息
enum message_type
{
    default_message_type = 0,

    // 绑定+心跳消息
    Bind_Control_Host2Slave,
    // 唤醒消息
    Wakeup_Control_Host2Slave,

    // 从机发送给主机的http 消息
    Slave2Host_UpdateTaskList_Request_Http,
    // 从机发送给主机的绑定消息
    Slave2Host_Bind_Request_Http,
    // 从机发送给主机的检查主机状态消息
    Slave2Host_Check_Host_Status_Request_Http,
    
    // 主机发送给从机的mqtt 消息
    Host2Slave_UpdateTaskList_Control_Mqtt,

    Feedback_ACK
};

struct MacAddress {
    uint8_t bytes[ESP_NOW_ETH_ALEN];
    bool operator==(const MacAddress& other) const {
        return memcmp(bytes, other.bytes, ESP_NOW_ETH_ALEN) == 0;
    }
};

// 定义ESP-NOW消息结构体
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];                   // 发送者的mac地址
    uint16_t unique_id;                   // 该条消息的唯一ID
    message_type m_message_type;          // 消息类型
    uint8_t data[MAX_EFFECTIVE_DATA_LEN]; // 有效数据
    uint8_t data_len;                     // 有效数据长度
} espnow_packet_t;

typedef struct {
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];                  // 发送者的mac地址
    uint8_t data_len;                    // 有效数据长度
} espnow_data_t;

typedef enum {
    default_role,
    host_role,
    slave_role,
} esp_now_role;

class EspNowClient{
    public:
        void init();
        esp_now_role m_role = default_role;
        void Addpeer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN]);
        void Delpeer(uint8_t peer_mac[ESP_NOW_ETH_ALEN]);
        void send_esp_now_message(uint8_t target_mac[ESP_NOW_ETH_ALEN], uint8_t* data, size_t len, message_type type, bool need_feedback, uint16_t unique_id);
        void send_ack_message(uint8_t target_mac[ESP_NOW_ETH_ALEN], uint16_t unique_id);
        void start_find_channel();
        void stop_find_channel();
        QueueHandle_t recv_message_queue;   // 接收消息队列
        QueueHandle_t recv_packet_queue;    // 接收数据包队列

        uint32_t m_recieve_packet_count = 0;
        bool is_connect_to_host = false;

        static EspNowClient* Instance() {
            static EspNowClient instance;
            return &instance;
        }

        TaskHandle_t update_task_handle = NULL;
        std::set<uint16_t> received_unique_ids;
        const size_t max_received_ids = 10;

        //打印uint8_t数组
        static void print_uint8_array(uint8_t *array, size_t length);
        static uint8_t crc_check(uint8_t *data, int len);
};

bool Is_connect_to_host();
void set_is_connect_to_host(bool _is_connect_to_host);

#endif