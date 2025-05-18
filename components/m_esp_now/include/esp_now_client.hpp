#ifndef ESP_NOW_CLIENT_HPP
#define ESP_NOW_CLIENT_HPP

#include "esp_now.h"
#include "esp_log.h"
#include "network.h"
#include "global_message.h"
#include "espnow.h"
#include "codec.hpp"
#include "esp_mac.h"

#define ESP_NOW "ESPNOW"
#define SCAN_CHANNEL_TIME_INTERVAL 2000
#define ESPNOW_SEND_MAX_TIMEOUT pdMS_TO_TICKS(2000)
#define Same_mac(mac1, mac2) (memcmp(mac1, mac2, ESP_NOW_ETH_ALEN) == 0)
//数据最大长度等于内置最大长度 - message类型
#define MAX_EFFECTIVE_DATA_LEN ESPNOW_SEC_PACKET_MAX_SIZE - 1
#define SLAVE_ASK_HOST_TIME 5
#define HOST_ASK_SLAVE_TIME 2

enum message_type
{
    default_message_type = 0, 

    // 主机发送给从机的绑定消息
    Host2Slave_Bind_Control_Http,
    Slave2Host_Bind_Request_Http,

    // 测试请求消息
    Test_Start_Request_Slave2Host,
    Test_Stop_Request_Slave2Host,
    Test_Feedback_Host2Slave,

    // 从机发送给主机的http消息

    Slave2Host_UpdateTaskList_Request_Http,
    Slave2Host_Get_Device_Info_Request_Http,
    Slave2Host_Get_Time_Request_Http,
    Slave2Host_Enter_Focus_Request_Http,
    Slave2Host_Out_Focus_Request_Http,
    Slave2Host_Sleep_Request_Http,
    Slave2Host_Wakeup_Request_Http,
    
    // 主机发送给从机的mqtt 消息
    Host2Slave_Bind_Control_Mqtt,
    Host2Slave_Device_Info_Control_Mqtt,
    Host2Slave_Get_Time_Control_Mqtt,
    Host2Slave_UpdateTaskList_Control_Mqtt,
    Host2Slave_Send_Task_List_Control_Mqtt,
    Host2Slave_Enter_Focus_Control_Mqtt,
    Host2Slave_Out_Focus_Control_Mqtt,

    // 反馈消息
    Feedback_ACK
};

//定义focus任务的结构体
typedef struct {
    uint8_t focus_type; //0表示默认，1表示纯时间任务，2表示任务，3表示录音任务
    int fallTiming;
    int focus_id;
    long long enter_focus_time;
    //对于task类型，需要记录task的名字
    uint8_t task_name_len;
    char task_name[100];
} focus_message_t;

focus_message_t pack_focus_message(uint8_t _focus_type, int _fallTiming, long long _enter_focus_time, int _focus_id, char* _task_name);
void focus_message_to_data(focus_message_t focus_message, uint8_t* data, size_t &data_len);
focus_message_t data_to_focus_message(uint8_t* data);

typedef enum {
    default_role,
    host_role,
    slave_role,
} esp_now_role;

// 外部统一接口
void set_is_connect_to_host(bool _is_connect_to_host);
bool Is_connect_to_host(void);

class EspNowClient{
    public:
        void init();
        static EspNowClient* Instance() {
            static EspNowClient instance;
            return &instance;
        }

    void print_uint8_array(uint8_t *array, size_t length)
    {
        char buffer[100];
        size_t offset = 0;
        for (size_t i = 0; i < length; i++) {
            // 将每个元素格式化为十六进制并追加到缓冲区
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", array[i]);
        }
        // 使用 ESP_LOGI 打印整个数组
        ESP_LOGI("data", "Array: %s\n", buffer);
    }
        
    bool is_connect_to_host = false;
    esp_now_role m_role = default_role;

    espnow_frame_head_t broadcast_head = {  
        .magic            = 0,  
        //如果不设置，则随机生成   
        .channel          = ESPNOW_CHANNEL_CURRENT,  
        //如果设定为ESPNOW_CHANNEL_ALL，分两种情况
        //如果开启了ack， 一旦收到 ACK，直接跳到 EXIT，退出所有循环不会接着轮询
        //如果没有开启ack，则会遍历所有信道  
        .filter_adjacent_channel = false,   
        //当这个包被从机收到了，会判断这个包的信道是否来自监听的信道，如果不是则不处理
        .filter_weak_signal      = false,
        //当接收设备接收到的信号低于 forward_rssi 时，帧头数据将被丢弃
        .security                = false,
        //是否加密
        .broadcast               = true,                
        //是否为广播，如果为广播则不会触发任何ack的内容，这个标会把espsend的dest_adrees强制变为广播地址
        // 单播 + ack + 任意缓冲   send_cb 成功（确定这个包发出去了才会开始检测ack） + 收到 magic ACK
        // 单播 + no ack   send_cb 成功
        // 广播 + ack      不合法（广播不会触发ack）
        // 广播 + no ack + 发送缓冲未满	  不等，直接返回
        // 广播 + no ack + 发送缓冲满	  会等 send_cb 再继续
        .group                 = false,
        //是否为组播,组播的本质还是广播，不支持ack
        //首先调用espnow_set_group(addrs_list, 10, group_id, &frame_head, true, portMAX_DELAY);
        // espnow_send(DATA_TYPE, group_id, data, size, &frame_head, portMAX_DELAY);
        // espnow_set_group(addrs_list, 10, group_id, &frame_head, false, portMAX_DELAY);
        // 第一条告诉所有设备：你们现在属于 group_id，
        // 第二条发组播数据，只有这些设备会处理，
        // 第三条让他们退出该组，释放资源。
        .ack                   = false,
        //是否开启ack
        .retransmit_count        = 1,   
        //重发的次数，当为单播且在没有收到ack               
        //1.发送的流程是从开始调用espnow_send时开始计时，在这段期间，只要没超过wait——ticks会按照(2,4,8,16,32,64,100,100,...)ms的区间循环发送和等待的流程
        //2.如果失败了，没有等到ack则按照retransmit_count重新开始一次发送，直到retransmit_count为0
        .forward_ttl           = 0,                  
        .forward_rssi          = -25,
        //只有满足：全局开启 forward_enable、广播包、TTL>0、信号≥阈值、且目标/源都不是自己，
        //则会转发一次，直到forward_ttl为0，并且由于是广播所以没有ack这一说
    };
    
    //需要ack
    espnow_frame_head_t target_send_head = {  
        .magic            = 0,  
        .channel          = ESPNOW_CHANNEL_CURRENT,  
        .filter_adjacent_channel = false,   
        .filter_weak_signal      = false,
        .security                = false,
        .broadcast               = false,                
        .group                 = false,
        .ack                   = true,
        .retransmit_count        = 10,   
        .forward_ttl           = 0,                  
        .forward_rssi          = 0,
    };

    //不需要ack但是要发送出去
    espnow_frame_head_t test_send_head = {  
        .magic            = 0,  
        .channel          = ESPNOW_CHANNEL_CURRENT,  
        .filter_adjacent_channel = false,   
        .filter_weak_signal      = false,
        .security                = false,
        .broadcast               = false,                
        .group                 = false,
        .ack                   = false,
        .retransmit_count        = 5,   
        .forward_ttl           = 0,                  
        .forward_rssi          = 0,
    };
    uint16_t test_connecting_send_count = 0;

    //espnow配置
    espnow_config_t espnow_config =  {  
    .pmk = {'E', 'S', 'P', '_', 'N', 'O', 'W', 0}, 
    .forward_enable = 0,  //是否开启转发
    .forward_switch_channel = 0,  //是否开启信道切换
    .sec_enable = 0,  //是否开启加密
    .reserved1 = 0,    //保留
    .qsize = 32,  //发送任务队列的大小
    .send_retry_num = 10,  //感觉像费案，没有任何地方用
    .send_max_timeout = ESPNOW_SEND_MAX_TIMEOUT,  
    //send_max_timeout只用来等待发送队列，一般wait_ticks 要 大于 send_max_timeout
    //wait_ticks 是整个 espnow_send() 调用的预算，包括了这段时间
    .receive_enable = { 
        .ack           = 1,  //是否开启ack
        .forward       = 0,  //是否开启转发
        .group         = 0,  //是否开启组播
        .provisoning   = 0,  //是否开启预设
        .control_bind  = 0,  //是否开启控制绑定
        .control_data  = 0,  //是否开启控制数据
        .ota_status    = 0,  //是否开启ota状态
        .ota_data      = 0, 
        .debug_log     = 0, 
        .debug_command = 0, 
        .data          = 0, 
        .sec_status    = 0, 
        .sec           = 0, 
        .sec_data      = 0, 
        .reserved2     = 0, 
        }, 
    };
    
    //用于切换扫描信道的任务
    TaskHandle_t update_task_handle = NULL;
    void start_find_channel();
    void stop_find_channel();   

    esp_err_t send_test_message(uint8_t* data, size_t size, const espnow_addr_t dest_addr)
    {
        uint8_t packet_data[size+1];
        packet_data[0] = default_message_type;
        memcpy(&packet_data[1], data, size);
        esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, packet_data,
                    size+1, &EspNowClient::Instance()->test_send_head, ESPNOW_SEND_MAX_TIMEOUT);
        return ret;
    }

    esp_err_t send_message(uint8_t* data, size_t size, message_type m_message_type, const espnow_addr_t dest_addr)
    {
        uint8_t packet_data[size+1];
        packet_data[0] = m_message_type;
        memcpy(&packet_data[1], data, size);
        esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, packet_data,
                    size+1, &EspNowClient::Instance()->target_send_head, ESPNOW_SEND_MAX_TIMEOUT);
        return ret;
    }
};
#endif