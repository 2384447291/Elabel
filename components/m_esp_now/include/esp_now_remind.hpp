#ifndef ESP_NOW_REMIND_HPP
#define ESP_NOW_REMIND_HPP
#include "MacAdrees.hpp"
#include "esp_now_client.hpp"
#define PINNING_TIMEOUT_MS 2000

enum host_send_state
{
    waiting, //正在等待开始pining
    pinning, //正在pining
    target_sending, //正在发送消息
};

class esp_now_remind_host
{
public:
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
    // 开始pinning的时间
    TickType_t pinning_start_time = 0;  
    // pin消息的唯一id
    uint16_t pin_message_unique_id = 0;

    //发送状态
    host_send_state send_state = waiting;

    void update();
    void get_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
    bool get_feedback_from_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN]);
    void Print_pin_situation();
};

#endif

