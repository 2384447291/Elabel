#include "global_message.h"
#include "esp_log.h"
#include "freertos/semphr.h"
//--------------------------------------TODOLIST 对应的结构体--------------------------------------//
SemaphoreHandle_t Global_message_mutex;

task_list_state m_task_list_state = newest;

task_list_state get_task_list_state()
{    
    return m_task_list_state;
};

void set_task_list_state(task_list_state _task_list_state)
{
    m_task_list_state = _task_list_state;
}

void cleantodoItem(TodoItem* _todoitem)
{
    _todoitem->createBy = NULL;
    _todoitem->createTime = 0;
    _todoitem->updateBy = NULL;
    _todoitem->remark = NULL;
    _todoitem->id = 0;
    _todoitem->title = NULL;
    _todoitem->isPressing = 0;
    _todoitem->todoType = NULL;
    _todoitem->isComplete = 0;
    _todoitem->startTime = 0;
    _todoitem->fallTiming = 0;
    _todoitem->isFocus = 0;
    _todoitem->isImportant = 0;    
}

// 根据 ID 查找 TodoItem
TodoItem* find_todo_by_id(TodoList *list, int id) 
{
    for (int i = 0; i < list->size; i++) {
        if (list->items[i].id == id) {
            return &list->items[i];
        }
    }
    ESP_LOGE("Task_list","Can not find todo_by_id/n.");
    return NULL; // 未找到返回 NULL
}

// 根据 Title 查找 TodoItem
TodoItem* find_todo_by_title(TodoList *list, const char *title) 
{
    if(title == NULL)
    {
        ESP_LOGE("Task_list","Can not find todo_by_id/n.");
        return NULL;
    } 
    for (int i = 0; i < list->size; i++) {
        if (list->items[i].title != NULL && strcmp(list->items[i].title, title) == 0) {
            return &list->items[i];
        }
    }
    ESP_LOGE("Task_list","Can not find todo_by_id/n.");
    return NULL; // 未找到返回 NULL
}

//为新的item申请内存
void copy_write_todo_item(TodoItem* src, TodoItem* dst)
{
    if(src->createBy!=NULL) dst->createBy = strdup(src->createBy);
    else dst->createBy = strdup("NULL");

    if(src->updateBy!=NULL) dst->updateBy = strdup(src->updateBy);
    else dst->updateBy = strdup("NULL");

    if(src->remark!=NULL) dst->remark = strdup(src->remark);
    else dst->remark = strdup("NULL");

    if(src->title!=NULL) dst->title = strdup(src->title);
    else dst->title = strdup("NULL");

    if(src->todoType!=NULL) dst->todoType = strdup(src->todoType);
    else dst->todoType = strdup("NULL");

    dst->createTime = src->createTime;
    dst->updateTime = src->updateTime;
    dst->id = src->id;
    dst->isPressing = src->isPressing;
    dst->isComplete = src->isComplete;
    dst->startTime = src->startTime;
    dst->fallTiming = src->fallTiming;
    dst->isFocus = src->isFocus;
    dst->isImportant = src->isImportant;
}

void clean_todo_list(TodoList *list)
{
    for(int i = 0; i<list->size; i++)
    {
        if(list->items[i].createBy!=NULL) free(list->items[i].createBy);
        if(list->items[i].title!=NULL) free(list->items[i].title);
        if(list->items[i].updateBy!=NULL) free(list->items[i].updateBy);
        if(list->items[i].remark!=NULL) free(list->items[i].remark);
        if(list->items[i].todoType!=NULL) free(list->items[i].todoType);
    }
    list->size = 0;
    free(list->items);
    list->items = NULL;
}

// 向 TodoList 添加一个 TodoItem
void add_or_update_todo_item(TodoList *list, TodoItem item) 
{
    if(item.isFocus == 1)
    {   
        if(get_global_data()->m_focus_state->is_focus != 1) 
        {
            get_global_data()->m_focus_state->is_focus = 1;
            get_global_data()->m_focus_state->focus_task_id = item.id;
            ESP_LOGI("Task_list", "A New focus Task show up, its number is %d, its id is %d.\n", list->size, item.id);
        }
    }

    list->items = (TodoItem *)realloc(list->items, (list->size + 1) * sizeof(TodoItem));
    //申请内存和指针NULL是两个东西。不写这一句，直接调用list->items[list->size].remark会报错
    list->items[list->size].remark = NULL;
    list->items[list->size].createBy = NULL;
    list->items[list->size].updateBy = NULL;
    list->items[list->size].todoType = NULL;
    list->items[list->size].title = NULL;
    if (list->items == NULL) 
    {
        ESP_LOGE("Task_list", "Memory allocation error!\n");
        return;
    }
    copy_write_todo_item(&item,&(list->items[list->size]));
    list->size++;
    ESP_LOGI("Task_list", "Add new item id is %d, title is %s ,total size of todolist is %d, create time is %lld, falling time is %d, focus state is %d.\n", list->items[list->size-1].id, list->items[list->size-1].title, list->size, item.startTime, item.fallTiming, item.isFocus);
}
//--------------------------------------TODOLIST 对应的结构体--------------------------------------//



//-------------------------------------- Global_data--------------------------------------//
// 静态变量，存储单例实例
static Global_data *instance = NULL;
// 获取单例实例的函数
Global_data* get_global_data() {
    // 如果实例还未创建，则创建它
    if (instance == NULL) 
    {
        instance = (Global_data*)malloc(sizeof(Global_data));
        if (instance != NULL) 
        {
            Global_message_mutex = xSemaphoreCreateMutex();
            instance->m_language = English;

            instance->m_focus_state = (Focus_state*)malloc(sizeof(Focus_state));
            instance->m_focus_state->is_focus = 0;
            instance->m_focus_state->focus_task_id = 0;

            instance->m_todo_list = (TodoList*)malloc(sizeof(TodoList));
            instance->m_todo_list->items = NULL;
            instance->m_todo_list->size = 0;

            memset(instance->m_mac_str, 0, sizeof(instance->m_mac_str));

            memset(instance->m_newest_firmware_url, 0, sizeof(instance->m_newest_firmware_url));    
            memset(instance->m_version, 0, sizeof(instance->m_version));
            memset(instance->m_deviceModel, 0, sizeof(instance->m_deviceModel));
            memset(instance->m_createTime, 0, sizeof(instance->m_createTime));
            
            memset(instance->m_usertoken, 0, sizeof(instance->m_usertoken));
            memset(instance->m_userName, 0, sizeof(instance->m_userName));
            memset(instance->m_wifi_password, 0, sizeof(instance->m_wifi_password));            
            memset(instance->m_wifi_ssid, 0, sizeof(instance->m_wifi_ssid));
        }
    }
    return instance;
}
//-------------------------------------- Global_data--------------------------------------//
