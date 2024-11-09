#ifndef M_ESP_NOW_H
#define M_ESP_NOW_H

#include "esp_now.h"
#include "global_message.h"

#define ESP_NOW "ESPNOW"
#define ESPNOW_RECIEVE_QUEUE_SIZE 64
#define ESPNOW_SEND_QUEUE_SIZE 64
//配对时候的基础信道
#define PEER_CHANEEL 7

#ifdef __cplusplus
extern "C" {
#endif
void m_espnow_init(void);
#ifdef __cplusplus
}
#endif

void Add_peer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN]);

typedef struct {
    uint8_t elabel_state;                 //elabel只有3种状态, 1.可操作（选task）2.别的label在操作中（设置倒计时）3.focus状倒计时态
    uint8_t chosenTaskId;                 //进入focus所选的任务的id
    uint32_t TimeCountdown;               //进入focus调用倒计时时间        
} __attribute__((packed)) m_state_synchronous_data;


//从标签只用显示task的名称并用任务id维护维护该任务
typedef struct {
    uint8_t task_type;                    //1：表示删除改任务，2：表示新增或者修改任务

    int changeTaskId;                 //任务ID
    uint8_t task_name_length;                  
    char task_name[100];
} __attribute__((packed)) m_task_synchronous_data;


typedef struct {
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];         //发给谁的
    uint8_t msg_type;                           //1：同步state变了，2：要更新task, 3:state和method都变了
    uint16_t seq_num;                           //该条同步命令的唯一序列号（随着发送累加）
    uint16_t crc;                               //CRC16校验 

    int len;                                    //发送buffer的长度
    m_state_synchronous_data state_data;       //状态同步的信息
    m_task_synchronous_data task_data;
}  m_espnow_event_send_cb_t;


typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];//谁发过来的
    uint8_t data[100];
    int data_len;
}  m_espnow_event_recv_cb_t;
#endif
