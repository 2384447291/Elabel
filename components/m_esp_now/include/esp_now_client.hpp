#ifndef ESP_NOW_CLIENT_HPP
#define ESP_NOW_CLIENT_HPP

#include "esp_now.h"
#include "esp_log.h"
#include "network.h"
#include "global_message.h"
#include "freertos/queue.h"

#define ESP_NOW "ESPNOW"
#define MAX_DATA_LEN 256
#define MAX_RECV_QUEUE_SIZE 10
//1s25个包
#define Esp_Now_Send_Interval 40
extern uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN];

// 定义ESP-NOW消息结构体
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN]; // 发送者的mac地址
    uint8_t data[MAX_DATA_LEN]; // 数据
    int data_len;
} espnow_message_t;

typedef struct {
    //是否被设置
    bool is_set = false;
    //重复发送的唯一ID
    uint8_t unique_id = 0;
    //发送的数据长度
    int data_len;
    //发送的数据
    uint8_t data[MAX_DATA_LEN];
    //这次发送还未收到反馈的从机
    uint8_t slave_num_not_feedback = 0;
    //这次发送还未收到反馈的从机
    uint8_t slave_mac_not_feedback[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
} remind_slave_t;

typedef enum {
    //绑定过程中的消息
    Bind_Control_Host2Slave = 25,
    Bind_Feedback_Slave2Host = 26,
    
    UpdateTaskList_Control_Host2Slave = 27,

    EnterFocus_Control_Host2Slave = 28,
    EnterFocus_Control_Slave2Host = 29,

    OutFocus_Control_Host2Slave = 30,
    OutFocus_Control_Slave2Host = 31,

    Feedback_Slave2Host = 32,

    Wakeup_Control_Host2Slave = 33,
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
        void send_esp_now_message(uint8_t target_mac[6], uint8_t* data, size_t len);
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
        TaskHandle_t host_send_update_task_handle = NULL;
    public:
        uint8_t slave_mac[MAX_SLAVE_NUM][ESP_NOW_ETH_ALEN];
        uint8_t slave_num;

        uint8_t unique_id = 0;

        remind_slave_t remind_slave;

        void empty_remind_slave()
        {
            remind_slave.is_set = false;
            remind_slave.unique_id = 0;
            remind_slave.data_len = 0;
            memset(remind_slave.data, 0, MAX_DATA_LEN);
            remind_slave.slave_num_not_feedback = 0;
            memset(remind_slave.slave_mac_not_feedback, 0, MAX_SLAVE_NUM * ESP_NOW_ETH_ALEN);
        }

        void get_feedback_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
        {
            if(remind_slave.is_set == false)
            {
                ESP_LOGE(ESP_NOW, "Get feedback slave failed, remind_slave is not set");
                return;
            }
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

            //如果slave_num_not_feedback为0,则说明所有从机都已近收到反馈,则可以删除remind_slave
            if(remind_slave.slave_num_not_feedback == 0)
            {
                empty_remind_slave();
            }
        }

        void add_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void del_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
        void init();
        void deinit();

        void send_wakeup_message();
        void send_broadcast_message();
        void send_update_task_list();
        void send_enter_focus(TodoItem _todo_item);
        void send_out_focus();

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
        void send_out_focus_message(int task_id, uint8_t unique_id);
        void send_enter_focus_message(int task_id, int fall_time, uint8_t unique_id);

        void send_feedcak_pack(uint8_t unique_id)
        {
            uint8_t pack[5];
            pack[0] = 0xAB;
            pack[1] = 0xCD;
            pack[2] = Feedback_Slave2Host;
            pack[3] = unique_id;
            pack[4] = 0xEF;
            client->send_esp_now_message(host_mac, pack, 5);
        }

        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }

        uint8_t unique_id = 0;
        EspNowSlave() {
            client = EspNowClient::Instance();
        }
};

#endif