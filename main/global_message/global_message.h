#ifndef GLOBAL_MESSAGE_H
#define GLOBAL_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define FIRMWARE_VERSION "2.0.5"
//--------------------------------------Focus 对应的结构体--------------------------------------//
typedef struct {
    int is_focus; //默认0，专注1，为专注2
    int focus_task_id;
} Focus_state;

//--------------------------------------Task_list对应的结构体--------------------------------------//
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

#ifdef __cplusplus
extern "C" 
{
#endif

task_list_state get_task_list_state(void);

void set_task_list_state(task_list_state _task_list_state);

//TodoItem初始化
void cleantodoItem(TodoItem* _todoitem);

// 根据 ID 查找 TodoItem
TodoItem* find_todo_by_id(TodoList *list, int id);

// 根据 Title 查找 TodoItem
TodoItem* find_todo_by_title(TodoList *list, const char *title);

// 向 TodoList 添加一个 TodoItem
void add_or_update_todo_item(TodoList *list, TodoItem item);

//清除 TodoList
void clean_todo_list(TodoList *list);
#ifdef __cplusplus
}
#endif

//-------------------------------------- Global_data--------------------------------------//
typedef struct 
{
    Focus_state* m_focus_state;//2表示否focus 1表示是focus 0表示没东西
    TodoList* m_todo_list;

    //为了确保字符串能够正确存储格式化后的 MAC 地址，并以 '\0' 结尾，字符串的大小应该至少为 18 字节。具体计算如下：
    // 每个字节以两位十六进制表示：02（2 字符）
    // 6 个字节的 MAC 地址：6 * 2 = 12 字符
    // 5 个冒号分隔符：5 字符
    // 1 个结束字符 '\0'：1 字符
    uint8_t mac_uint[6];
    char mac_str[18];

    //查询的最新版本号
    char* newest_firmware_url;
    char *version;
    char *deviceModel;
    char *createTime;
    //查询的用户名称
    char* usertoken;
    char* userName;
    //连接的wifi的名称和密码
    char* wifi_ssid;
    char* wifi_password;
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