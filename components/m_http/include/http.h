#ifndef HTTP_H
#define HTTP_H

#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"

#define MAX_TASK_SIZE 64
#define HTTP_TAG "HTTP"
#define MAX_PARA 10

typedef enum {
    NO_TASK,
    ADD_ENTER_FOCUS,
    ENTER_FOCUS,
    OUT_FOCUS,
    DELETTODO,
    ADDTODO,
    FINDLATESTVERSION,
    FINDTODOLIST,
    FINDUSER,
    BINDDEVICE,
    FINDDEVICE,
    UNBINDDEVICE,
    SAVESETTING,
    SAVEPOWER,
} http_task_t;

char* taskToString(http_task_t task);

typedef enum {
    send_waiting,
    send_processing,
    send_success,
    send_fail,
} http_state;

typedef struct {
    uint64_t unique_id;
    http_task_t task;
    char *parament[MAX_PARA];
    bool need_stuck;
    bool is_suceess;
}http_task_struct;
//--------------------------------------TaskQueue--------------------------------------//
typedef struct {
    http_task_struct* data[MAX_TASK_SIZE];        // 存储队列元素
    int front;                        // 队头索引
    int rear;                         // 队尾索引
} TaskQueue;

bool isEmpty(TaskQueue *q);
bool isFull(TaskQueue *q);
bool enqueue(TaskQueue *q, http_task_struct *task);
bool enqueue_front(TaskQueue *q, http_task_struct *task);
bool dequeue(TaskQueue* q, http_task_struct* dealing_task);
esp_http_client_handle_t* get_client(void);
esp_http_client_config_t* get_config(void);
http_state* get_m_http_state(void);
bool get_need_deal_with_music(void);
void set_need_deal_with_music(bool need_deal);
http_task_struct *create_http_task_struct(http_task_t task_type, char *params[], int param_count, bool need_stuck);
void initQueue(TaskQueue *q);
void printTaskList(TaskQueue *q);
//--------------------------------------TaskQueue--------------------------------------//

#ifdef __cplusplus
extern "C" {
#endif

void http_client_init(void);

bool http_get_latest_version(bool need_stuck);

bool http_add_to_do(char *title, char*todoType, bool need_stuck);

bool http_add_enter_focus(char *title, char*todoType, int fallingTime, bool need_stuck);

bool http_get_todo_list(bool need_stuck);

bool http_in_focus(char *id, int fallingTime, bool need_stuck);

bool http_out_focus(char *id,bool need_stuck);

bool http_delet_todo(char *id,bool need_stuck);

bool http_bind_device(bool need_stuck, uint8_t mac[6]);

bool http_unbind_device(bool need_stuck, uint8_t mac[6]);

bool http_find_device(bool need_stuck);

bool http_find_usr(bool need_stuck);

bool http_save_setting(bool need_stuck, char *setting, uint8_t mac[6]);

bool http_save_power(bool need_stuck, int32_t power, uint8_t mac[6]);
#ifdef __cplusplus
}
#endif

#endif

