#ifndef GLOBAL_MESSAGE_H
#define GLOBAL_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"

#define MAX_SLAVE_NUM 6
#define FIRMWARE_VERSION "3.0.8"
//--------------------------------------Focus 对应的结构体--------------------------------------//
typedef struct {
    int is_focus; //默认0，专注1，未专注2
    int focus_task_id;
} Focus_state;

//--------------------------------------Task_list对应的结构体--------------------------------------//
typedef enum {
    English,
    Chinese,
} language;

typedef enum {
    newest,
    updating_from_server,
    firmware_need_update,
} task_list_state;

typedef struct {
    char *createBy;
    long long createTime;
    char *updateBy;
    long long updateTime;
    char *remark;
    int id;
    char *title;
    int isPressing;
    char *todoType;
    int taskType; //0表示默认，1表示纯时间任务，2表示任务，3表示录音任务
    int isComplete;
    long long startTime;
    int fallTiming;
    int isFocus;
    int isImportant;
} TodoItem;

typedef struct {
    TodoItem *items;  // 动态数组存储 TodoItem
    int size;         // 当前数组中的元素数量
} TodoList;

typedef struct {
    int power;//-1表示正在充电，0-100表示电量
    bool is_idel_clock_time;
    uint8_t default_counter_time;
    uint8_t overtime_alert_time;
    uint8_t sound_volume;
    uint8_t sleep_time;
} device_info;

typedef struct {
    uint8_t mac[6];
    device_info setting;
    bool is_sleep;
} slave_device_info;

#ifdef __cplusplus
extern "C" 
{
#endif

task_list_state get_task_list_state(void);

void set_task_list_state(task_list_state _task_list_state);

//TodoItem初始化
void cleantodoItem(TodoItem* _todoitem);

// 拷贝 TodoItem
void copy_write_todo_item(TodoItem* src, TodoItem* dst);

// 根据 ID 查找 TodoItem
TodoItem* find_todo_by_id(TodoList *list, int id);

// 根据 Title 查找 TodoItem
TodoItem* find_todo_by_title(TodoList *list, const char *title);

// 向 TodoList 添加一个 TodoItem
void add_or_update_todo_item(TodoList *list, TodoItem item);

//清除 TodoList
void clean_todo_list(TodoList *list);

bool insert_slave(uint8_t slave_mac[6]);

bool delete_slave(uint8_t slave_mac[6]);

uint8_t set_sleep(uint8_t slave_mac[6], bool is_sleep);
#ifdef __cplusplus
}
#endif

//-------------------------------------- Global_data--------------------------------------//
typedef struct 
{
    //语言
    language m_language;
    //主机还是从机 0 是没有设定 1 是主机 2 是从机
    uint8_t m_is_host;
    //是否有专注任务
    Focus_state* m_focus_state;//2表示否focus 1表示是focus 0表示没东西
    TodoList* m_todo_list;

    //为了确保字符串能够正确存储格式化后的 MAC 地址，并以 '\0' 结尾，字符串的大小应该至少为 18 字节。具体计算如下：
    // 每个字节以两位十六进制表示：02（2 字符）
    // 6 个字节的 MAC 地址：6 * 2 = 12 字符
    // 5 个冒号分隔符：5 字符
    // 1 个结束字符 '\0'：1 字符
    uint8_t m_mac_uint[6];
    char m_mac_str[18];

    //当前设备的状态
    device_info m_device_info;

    //查询的最新版本号
    char m_newest_firmware_url[100];
    char m_version[100];
    char m_deviceModel[100];
    char m_createTime[100];
    //查询的用户名称
    char m_usertoken[100];
    char m_userName[100];
    //连接的wifi的名称和密码
    char m_wifi_ssid[100];    
    char m_wifi_password[100];

    //如果是从机保存的主机mac
    uint8_t m_host_mac[6];
    uint8_t m_host_channel;
    //如果是主机保存的从机mac
    slave_device_info m_slave_info[MAX_SLAVE_NUM];
    uint8_t m_slave_num;
    bool need_update_device_info;
    
} Global_data;

#ifdef __cplusplus
extern "C" {
#endif
// 获取单例实例的函数
Global_data* get_global_data();
#ifdef __cplusplus
}
#endif


#endif