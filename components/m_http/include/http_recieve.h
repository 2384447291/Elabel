#ifndef HTTP_RECIEIVE_H
#define HTTP_RECIEIVE_H
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "http.h"
#include "global_message.h"
#include "esp_mac.h"
// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 
void parse_json_response(char *response, http_task_struct *m_task_struct, http_state *m_http_state) 
{
    // ESP_LOGI(HTTP_TAG, "Full response: %s", response);
    // 解析 JSON
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE("HTTP","JSON parse error!\n");
        return;
    }
    else{
        // ESP_LOGI("HTTP", "Start parse response\n");
    };

    // 获取 msg 字段
    cJSON *msg = cJSON_GetObjectItem(json, "msg");
    if (cJSON_IsString(msg) && (msg->valuestring != NULL)) {
        // ESP_LOGI("HTTP", "Message: %s", msg->valuestring);
    }

    // 获取 code 字段
    cJSON *code = cJSON_GetObjectItem(json, "code");
    if (cJSON_IsNumber(code)) {
        // ESP_LOGI("HTTP", "Code: %d", code->valueint);
        if(code->valueint != 200)
        {
            *m_http_state = send_fail;
            return;
        }
    }
    if(m_task_struct->task == FINDTODOLIST)
    {
        // 获取 rows 数组
        cJSON *data = cJSON_GetObjectItem(json, "rows");
        int array_size = cJSON_GetArraySize(data);
        clean_todo_list(get_global_data()->m_todo_list);
        for (int i = 0; i < array_size; i++) 
        {
            cJSON *item = cJSON_GetArrayItem(data, i);
            TodoItem todo;
            cleantodoItem(&todo);
            // 解析各字段
            todo.createBy = cJSON_GetStringValue(cJSON_GetObjectItem(item, "createBy"));

            char *create_time_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "createTime"));
            if (create_time_str != NULL) {
                todo.createTime = strtoll(create_time_str, NULL, 10);
            }

            todo.updateBy = cJSON_GetStringValue(cJSON_GetObjectItem(item, "updateBy"));

            char *updateTime_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "updateTime"));
            if (updateTime_str != NULL) {

                todo.updateTime = strtoll(updateTime_str, NULL, 10);
            }

            todo.remark = cJSON_GetStringValue(cJSON_GetObjectItem(item, "remark"));

            todo.id = cJSON_GetObjectItem(item, "id")->valueint;

            todo.title = cJSON_GetStringValue(cJSON_GetObjectItem(item, "title"));

            todo.isPressing = cJSON_GetObjectItem(item, "isPressing")->valueint;

            todo.todoType = cJSON_GetStringValue(cJSON_GetObjectItem(item, "todoType"));

            todo.taskType = cJSON_GetObjectItem(item, "taskType")->valueint;

            todo.isComplete = cJSON_GetObjectItem(item, "isComplete")->valueint;

            char *startTime_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "startTime"));
            if (startTime_str != NULL) {
                todo.startTime = strtoll(startTime_str, NULL, 10);
            }

            char *fallTiming_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "fallTiming"));
            if (fallTiming_str != NULL) {
                todo.fallTiming = atoi(fallTiming_str);
            }

            todo.isFocus = cJSON_GetObjectItem(item, "isFocus")->valueint;

            todo.isImportant = cJSON_GetObjectItem(item, "isImportant")->valueint;

            Global_data* _global_data = get_global_data();
            add_or_update_todo_item(_global_data->m_todo_list, todo);
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDTODOLIST\n");
    }
    else if(m_task_struct->task == FINDLATESTVERSION)
    {   
       // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if(data->valuestring == NULL){
            ESP_LOGI("HTTP", "This version is newest");
        }
        else{
            cJSON *setting = cJSON_GetObjectItem(data, "data");
            if (setting != NULL) {
                
            }
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDLATESTVERSION\n");        
    }
    else if (m_task_struct->task == FINDUSER)
    {
        // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL) 
        {
            const char *userName = cJSON_GetStringValue(cJSON_GetObjectItem(data, "userName"));
            memcpy(get_global_data()->m_userName, userName, strlen(userName));
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDUSER.\n ");
    } 
    else if (m_task_struct->task == FINDDEVICE)
    {
        // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL) 
        {
            int array_size = cJSON_GetArraySize(data);
            for(int i = 0; i < array_size; i++)
            {
                cJSON *item = cJSON_GetArrayItem(data, i);
                // 获取 setting 对象
                const char *setting_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "setting"));
                uint8_t mac[6];
                device_info setting_info;
                if (setting_str != NULL) {
                    char temp[4] = {0};  // 临时缓冲区
                    // 解析 default_counter (005)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str);
                    setting_info.default_counter_time = atoi(temp);

                    // 解析 overtime_freq (010)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str + 3);
                    setting_info.overtime_alert_time = atoi(temp);

                    // 解析 idle_clock (1)
                    snprintf(temp, sizeof(temp), "%.1s", setting_str + 6);
                    setting_info.is_idel_clock_time = atoi(temp);

                    // 解析 volume (80)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str + 7);
                    setting_info.sound_volume = atoi(temp);
                }
                
                // 获取并转换 sn 为 MAC 地址
                const char *sn_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "sn"));
                if (sn_str != NULL) {
                    // 将字符串形式的 MAC 地址转换为字节数组

                    sscanf(sn_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
                    
                    // 打印 MAC 地址用于调试
                    ESP_LOGI("HTTP", "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                }

                // 检测是否位主机
                if(memcmp(get_global_data()->m_mac_uint, mac, 6) == 0)
                {
                    get_global_data()->m_device_info.default_counter_time = setting_info.default_counter_time;
                    get_global_data()->m_device_info.overtime_alert_time = setting_info.overtime_alert_time;
                    get_global_data()->m_device_info.is_idel_clock_time = setting_info.is_idel_clock_time;
                    get_global_data()->m_device_info.sound_volume = setting_info.sound_volume;
                    ESP_LOGI("HTTP", "Save setting to host " MACSTR ", default_counter_time is %d, overtime_alert_time is %d, is_idel_clock_time is %d, sound_volume is %d", MAC2STR(mac), get_global_data()->m_device_info.default_counter_time, get_global_data()->m_device_info.overtime_alert_time, get_global_data()->m_device_info.is_idel_clock_time, get_global_data()->m_device_info.sound_volume);
                }
                //查找是否位从机数据
                else
                {
                    for(int i = 0; i < get_global_data()->m_slave_num; i++)
                    {
                        if(memcmp(get_global_data()->m_slave_info[i].mac, mac, 6) == 0)
                        {
                            get_global_data()->m_slave_info[i].setting.default_counter_time = setting_info.default_counter_time;
                            get_global_data()->m_slave_info[i].setting.overtime_alert_time = setting_info.overtime_alert_time;
                            get_global_data()->m_slave_info[i].setting.is_idel_clock_time = setting_info.is_idel_clock_time;
                            get_global_data()->m_slave_info[i].setting.sound_volume = setting_info.sound_volume;
                            ESP_LOGI("HTTP", "Save setting to slave " MACSTR ", default_counter_time is %d, overtime_alert_time is %d, is_idel_clock_time is %d, sound_volume is %d", MAC2STR(mac), get_global_data()->m_slave_info[i].setting.default_counter_time, get_global_data()->m_slave_info[i].setting.overtime_alert_time, get_global_data()->m_slave_info[i].setting.is_idel_clock_time, get_global_data()->m_slave_info[i].setting.sound_volume);
                        }
                    }
                }
            }
            get_global_data()->need_update_device_info = true;
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDDEVICE.\n ");
    }
    else if (m_task_struct->task == ADDTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ADDTODO title is %s, todo type is %s.\n",m_task_struct->parament[0],m_task_struct->parament[1]);
    }   
    else if (m_task_struct->task == ADD_ENTER_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ADD_ENTER_FOCUS title is %s, todo type is %s, falling time is %s.\n",m_task_struct->parament[0],m_task_struct->parament[1],m_task_struct->parament[2]);
    }
    else if (m_task_struct->task == ENTER_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ENTER_FOCUS Task %s enter focus\n",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == OUT_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is OUT_FOCUS Task %s out focus\n",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == DELETTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is DELETTODO Task %s is deleted\n",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == BINDDEVICE)
    {
        ESP_LOGI("HTTP", "Successful get response post task is BINDDEVICE.\n ");
    } 

    else if (m_task_struct->task == UNBINDDEVICE)
    {
        ESP_LOGI("HTTP", "Successful get response post task is UNBINDDEVICE.\n ");
    }
    else if (m_task_struct->task == SAVESETTING)
    {
        ESP_LOGI("HTTP", "Successful get response post task is SAVESETTING.\n ");
    }
    else if (m_task_struct->task == SAVEPOWER)
    {
        ESP_LOGI("HTTP", "Successful get response post task is SAVEPOWER.\n ");
    }
    // 释放 JSON 对象
    cJSON_Delete(json);
    if(m_task_struct->need_stuck)
    {
        m_task_struct->need_stuck = false;
    }
    *m_http_state = send_waiting;
}
#endif