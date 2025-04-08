#ifndef ESP_NOW_REMIND_HPP
#define ESP_NOW_REMIND_HPP
#include "MacAdrees.hpp"
#include "esp_now_client.hpp"
#define MAX_FEEDBACK_COUNT 20
#define SEND_DELAY_UNIT_MSECS 2
#define PINNING_TIME_MSECS 10000
#define SENDING_TIME_MSECS 2000

enum espnow_event_status
{
    //通用的
    ESPNOW_EVENT_DEFAULT,
    ESPNOW_EVENT_FEEDBACK_TIME_OUT,
    ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED,

    //给pining用的
    ESPNOW_EVENT_PINING_NOBODY_ONLINE,
    ESPNOW_EVENT_PINING_SOMEBODY_ONLINE,
    ESPNOW_EVENT_PINING_ALL_ONLINE,

    //给send用的
    ESPNOW_EVENT_SENDING_SUCCESS,
    ESPNOW_EVENT_SENDING_TIME_OUT,

    ESPNOW_EVENT_SEND_BUSY,
};

struct feedback_message_t
{
    uint8_t mac[ESP_NOW_ETH_ALEN];
    uint16_t unique_id;
};

class esp_now_remind
{
public:
    //如果是主机则是广播，如果是从机则是主机mac
    uint8_t send_mac[ESP_NOW_ETH_ALEN];

    //是否正在运行
    bool is_running = false;
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

    QueueHandle_t feedback_queue = xQueueCreate(MAX_FEEDBACK_COUNT, sizeof(feedback_message_t));

    espnow_event_status check_feedback(MacAddress &target_mac, uint16_t &unique_id, uint8_t &count); 
    espnow_event_status check_pining(MacAddress& Target_slave_mac, MacAddress& Online_slave_mac, uint16_t &unique_id, uint8_t &count);
    espnow_event_status esp_now_send(uint8_t __aligned[ESP_NOW_ETH_ALEN], uint8_t* _data, size_t _data_len, 
                                    message_type _m_message_type, MacAddress _target_mac, bool _need_pinning);

    void Print_pin_situation()
    {
        if (Online_slave_mac.count != Target_slave_mac.count) 
        {
            ESP_LOGE(ESP_NOW, "Pinning timeout!");
            // 打印在线从机
            ESP_LOGE(ESP_NOW, "Online slaves (%d):", Online_slave_mac.count);
            for(size_t i = 0; i < Online_slave_mac.count; i++) {
                ESP_LOGE(ESP_NOW, "  - " MACSTR "", MAC2STR(Online_slave_mac.bytes[i]));
            }
            
            // 找出未回复的从机
            MacAddress offline_slaves;
            for(size_t i = 0; i < Target_slave_mac.count; i++) 
            {   
                if(!Online_slave_mac.exists(Target_slave_mac.bytes[i])) {
                    offline_slaves.insert(Target_slave_mac.bytes[i]);
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

