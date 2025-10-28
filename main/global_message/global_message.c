#include "global_message.h"
#include "esp_log.h"
#include "freertos/semphr.h"

//--------------------------------------Slave_info 对应的结构体--------------------------------------//

bool mac_address_exists(const uint8_t mac[6]) {
    for (size_t i = 0; i < get_global_data()->m_slave_num; ++i) {
        if (memcmp(get_global_data()->m_slave_info[i].mac, mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

bool insert_slave(uint8_t slave_mac[6])
{
    if (get_global_data()->m_slave_num >= MAX_SLAVE_NUM) {
        ESP_LOGW("slave_info", "Cannot add more slaves, reached maximum limit of %d", MAX_SLAVE_NUM);
        return false;
    }

    if (mac_address_exists(slave_mac)) {
        ESP_LOGW("slave_info", "MAC "MACSTR" address already exists", MAC2STR(slave_mac));
        return false;
    }

    memcpy(get_global_data()->m_slave_info[get_global_data()->m_slave_num].mac, slave_mac, 6);
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].is_sleep = false;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.default_counter_time = 5;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.overtime_alert_time = 10;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.is_idel_clock_time = 1;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.is_strong_wake_up = 1;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.sound_volume = 80;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.power = 0;
    get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.sleep_time = 10;
    memset(get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.language, 0, sizeof(get_global_data()->m_slave_info[get_global_data()->m_slave_num].setting.language));
    get_global_data()->m_slave_num++;
    ESP_LOGI("slave_info", "Add new slave, its mac is "MACSTR" ", MAC2STR(slave_mac));
    return true;
}

bool delete_slave(uint8_t slave_mac[6])
{
    if (!mac_address_exists(slave_mac)) {
        ESP_LOGW("slave_info", "MAC "MACSTR" address does not exist", MAC2STR(slave_mac));
        return false;
    }

    // 找到要删除的从设备的索引
    size_t delete_index = 0;
    for (size_t i = 0; i < get_global_data()->m_slave_num; ++i) {
        if (memcmp(get_global_data()->m_slave_info[i].mac, slave_mac, 6) == 0) {
            delete_index = i;
            break;
        }
    }

    // 将后面的元素向前移动
    for (size_t i = delete_index; i < get_global_data()->m_slave_num - 1; ++i) {
        memcpy(&get_global_data()->m_slave_info[i], &get_global_data()->m_slave_info[i + 1], sizeof(slave_device_info));
    }

    // 减少从设备数量
    get_global_data()->m_slave_num--;
    ESP_LOGI("slave_info", "Delete slave with MAC "MACSTR"", MAC2STR(slave_mac));
    return true;
}

//0表示没有找到，1表示不需要更新，2表示更新了
uint8_t set_sleep(uint8_t slave_mac[6], bool is_sleep)
{
    if (!mac_address_exists(slave_mac)) {
        ESP_LOGW("slave_info", "MAC "MACSTR" address does not exist", MAC2STR(slave_mac));
        return 0;
    }

    // 找到对应的从设备并设置睡眠状态
    for (size_t i = 0; i < get_global_data()->m_slave_num; ++i) {
        if (memcmp(get_global_data()->m_slave_info[i].mac, slave_mac, 6) == 0) {
            if(is_sleep == get_global_data()->m_slave_info[i].is_sleep)
            {
                return 1;
            }
            get_global_data()->m_slave_info[i].is_sleep = is_sleep;
            ESP_LOGI("slave_info", "Set slave "MACSTR" sleep state to %d", MAC2STR(slave_mac), is_sleep);
            return 2;
        }
    }
    return 0;
}
//--------------------------------------Slave_info 对应的结构体--------------------------------------//
//--------------------------------------TODOLIST 对应的结构体--------------------------------------//
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
    _todoitem->updateTime = 0;
    _todoitem->remark = NULL;
    _todoitem->id = 0;
    _todoitem->title = NULL;
    _todoitem->isPressing = 0;
    _todoitem->todoType = NULL;
    _todoitem->taskType = 0;
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
    ESP_LOGE("Task_list","Can not find todo_by_id\n.");
    return NULL; // 未找到返回 NULL
}

// 根据 Title 查找 TodoItem
TodoItem* find_todo_by_title(TodoList *list, const char *title) 
{
    if(title == NULL)
    {
        ESP_LOGE("Task_list","Can not find todo_by_id\n.");
        return NULL;
    } 
    for (int i = 0; i < list->size; i++) {
        if (list->items[i].title != NULL && strcmp(list->items[i].title, title) == 0) {
            return &list->items[i];
        }
    }
    ESP_LOGE("Task_list","Can not find todo_by_id\n.");
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
    dst->taskType = src->taskType;
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

// 向 TodoList 添加一个 TodoItem，使用的是内存拷贝，申请一片新的内存，完全复制TodoItem
void add_or_update_todo_item(TodoList *list, TodoItem item) 
{
    if(item.isFocus == 1)
    {   
        get_global_data()->m_focus_state->is_focus = 1;
        get_global_data()->m_focus_state->focus_task_id = item.id;
        ESP_LOGI("Task_list", "A New focus Task show up, its title is %s, its id is %d.\n", item.title, item.id);
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
    ESP_LOGI("Task_list", "Add new item id is %d, title is %s ,total size of todolist is %d, create time is %lld, falling time is %d, foucs_type is %d, Tasktype is %d.\n", list->items[list->size-1].id, list->items[list->size-1].title, list->size, item.startTime, item.fallTiming, item.isFocus, item.taskType);
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
            instance->reset_count = 0;
            instance->m_is_host = 0;

            instance->m_focus_state = (Focus_state*)malloc(sizeof(Focus_state));
            instance->m_focus_state->is_focus = 0;
            instance->m_focus_state->focus_task_id = 0;
            instance->focusing_task_id = 0;

            instance->m_todo_list = (TodoList*)malloc(sizeof(TodoList));
            instance->m_todo_list->items = NULL;
            instance->m_todo_list->size = 0;

            memset(instance->m_mac_uint,0, sizeof(instance->m_mac_uint));
            memset(instance->m_mac_str, 0, sizeof(instance->m_mac_str));
            
            instance->m_device_info.default_counter_time = 5;
            instance->m_device_info.overtime_alert_time = 10;
            instance->m_device_info.is_idel_clock_time = 1;
            instance->m_device_info.is_strong_wake_up = 1;
            instance->m_device_info.sound_volume = 80;
            instance->m_device_info.power = 0;
            instance->m_device_info.sleep_time = 0;

            memset(instance->m_newest_firmware_url, 0, sizeof(instance->m_newest_firmware_url));    
            memset(instance->m_content, 0, sizeof(instance->m_content));
            memset(instance->m_version, 0, sizeof(instance->m_version));
            memset(instance->m_deviceModel, 0, sizeof(instance->m_deviceModel));
            memset(instance->m_createTime, 0, sizeof(instance->m_createTime));
            
            memset(instance->m_usertoken, 0, sizeof(instance->m_usertoken));
            memset(instance->m_userName, 0, sizeof(instance->m_userName));

            memset(instance->m_wifi_password, 0, sizeof(instance->m_wifi_password));            
            memset(instance->m_wifi_ssid, 0, sizeof(instance->m_wifi_ssid));

            memset(instance->m_host_mac,0, sizeof(instance->m_host_mac));
            instance->m_host_channel = 0;

            memset(instance->m_slave_info,0, sizeof(instance->m_slave_info)); 
            instance->m_slave_num = 0;
            instance->need_update_device_info = false;
            instance->need_update_device = false;
            instance->unbind_device_num = 0;
            memset(instance->unbind_device_mac, 0, sizeof(instance->unbind_device_mac));
        }
    }
    return instance;
}
//-------------------------------------- Global_data--------------------------------------//



