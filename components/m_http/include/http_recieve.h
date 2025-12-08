#ifndef HTTP_RECIEIVE_H
#define HTTP_RECIEIVE_H
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "http.h"
#include "global_message.h"
#include "global_nvs.h"
#include "esp_mac.h"
void parse_json_response(char *response, http_task_struct *m_task_struct, http_state *m_http_state) 
{
    // ESP_LOGI(HTTP_TAG, "Full response: %s", response);
    // 解析 JSON
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        // 释放 JSON 对象
        cJSON_Delete(json);
        *m_http_state = send_fail;
        ESP_LOGE(HTTP_TAG, "Full response: %s", response);
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
            // 释放 JSON 对象
            cJSON_Delete(json);
            *m_http_state = send_fail;
            ESP_LOGE(HTTP_TAG, "Full response: %s", response);
            return;
        }
    }
    
    if(m_task_struct->task == FINDTODOLIST)
    {
        // 获取 data 数组
        cJSON *data = cJSON_GetObjectItem(json, "data");
        int todo_type_array_size = cJSON_GetArraySize(data);
        clean_todo_list(get_global_data()->m_todo_list);
        // 有这么多类型的datatype
        for (int i = 0; i < todo_type_array_size; i++) 
        {
            //获取第一个todotype类型组
            cJSON *todotype = cJSON_GetArrayItem(data, i);
            cJSON *todolist = cJSON_GetObjectItem(todotype, "todoList");
            int todo_array_size = cJSON_GetArraySize(todolist);
            for (int j = 0; j < todo_array_size; j++) 
            {
                cJSON *item = cJSON_GetArrayItem(todolist, j);
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
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDTODOLIST");
    }
    else if(m_task_struct->task == FINDLATESTVERSION)
    {   
       // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL && !cJSON_IsNull(data)) 
        {
            const char *version = cJSON_GetStringValue(cJSON_GetObjectItem(data, "version"));
            const char *deviceModel = cJSON_GetStringValue(cJSON_GetObjectItem(data, "deviceModel"));
            const char *newest_firmware_url = cJSON_GetStringValue(cJSON_GetObjectItem(data, "firmwareUrl"));
            const char *createTime = cJSON_GetStringValue(cJSON_GetObjectItem(data, "createTime"));
            const char *language = cJSON_GetStringValue(cJSON_GetObjectItem(data, "language"));
            const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(data, "content"));
            if (version) memcpy(get_global_data()->m_version, version, strlen(version));
            if (deviceModel) memcpy(get_global_data()->m_deviceModel, deviceModel, strlen(deviceModel));
            if (newest_firmware_url) memcpy(get_global_data()->m_newest_firmware_url, newest_firmware_url, strlen(newest_firmware_url));
            if (createTime) memcpy(get_global_data()->m_createTime, createTime, strlen(createTime));
            if (content) memcpy(get_global_data()->m_content, content, strlen(content));
            ESP_LOGI("HTTP", "Successful get response post task is FINDLATESTVERSION," 
                        "version is %s," 
                        "deviceModel is %s," 
                        "newest_firmware_url is %s," 
                        "createTime is %s," 
                        "language is %s," 
                        "content is %s", 
                        version, deviceModel, newest_firmware_url, createTime, language, content);   
        } 
        else 
        {
            ESP_LOGI("HTTP", "Newest firmware no need update");
        }       
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
        ESP_LOGI("HTTP", "Successful get response post task is FINDUSER ");
    } 
    else if (m_task_struct->task == FINDDEVICE)
    {
        // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL) 
        {
            int array_size = cJSON_GetArraySize(data);

            //--------------检测是否所有设备还在线----------------//
            bool Is_get_slave_device_Info[get_global_data()->m_slave_num];
            for(int i = 0; i < get_global_data()->m_slave_num; i++)
            {
                Is_get_slave_device_Info[i] = false;
            }
            bool Is_get_host_device_Info = false;
            get_global_data()->unbind_device_num = 0;
            memset(get_global_data()->unbind_device_mac, 0, sizeof(get_global_data()->unbind_device_mac));
            //--------------检测是否所有设备还在线----------------//

            for(int i = 0; i < array_size; i++)
            {
                cJSON *item = cJSON_GetArrayItem(data, i);
                // 获取 setting 对象
                const char *setting_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "setting"));
                uint8_t mac[6];
                device_info setting_info;
                if (setting_str != NULL) 
                {
                    ESP_LOGI("HTTP", "setting_str is %s", setting_str);
                    char temp[12] = {0};  // 临时缓冲区
                    // 解析 default_counter (005)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str);
                    setting_info.default_counter_time = atoi(temp);

                    // 解析 overtime_freq (010)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str + 3);
                    setting_info.overtime_alert_time = atoi(temp);

                    // 解析 idle_clock (1)
                    snprintf(temp, sizeof(temp), "%.1s", setting_str + 6);
                    setting_info.is_idel_clock_time = atoi(temp);

                    // 解析 volume (080)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str + 7);
                    setting_info.sound_volume = atoi(temp);

                    //解析 sleep_cycle (030)
                    snprintf(temp, sizeof(temp), "%.3s", setting_str + 10);
                    setting_info.sleep_time = atoi(temp);

                    // 解析 strong_wake_up (1)
                    snprintf(temp, sizeof(temp), "%.1s", setting_str + 13);
                    setting_info.is_strong_wake_up = atoi(temp);

                    // 解析 language (如: 00501010800300-zh)
                    memset(setting_info.language, 0, sizeof(setting_info.language));
                    const char *dash = strchr(setting_str, '-');
                    if (dash != NULL && *(dash + 1) != '\0') 
                    {
                        // 拷贝 "-" 后的内容到 language，最多保留 11 字节并手动结尾
                        strncpy(setting_info.language, dash + 1, sizeof(setting_info.language) - 1);
                        setting_info.language[sizeof(setting_info.language) - 1] = '\0';
                        
                        // 转换为大写 (en -> EN, En -> EN, EN -> EN)，ota部分请求为大写，后端改的是小写，我这边上传的是大写
                        for (int i = 0; setting_info.language[i] != '\0'; i++) 
                        {
                            if (setting_info.language[i] >= 'a' && setting_info.language[i] <= 'z')
                            {
                                setting_info.language[i] = setting_info.language[i] - 'a' + 'A';
                            }
                        }
                    } 
                    else 
                    {
                        ESP_LOGE("HTTP", "No language suffix found, setting default to EN");
                        strncpy(setting_info.language, "EN", sizeof(setting_info.language) - 1);
                        setting_info.language[sizeof(setting_info.language) - 1] = '\0';
                    }
                }
                
                // 获取并转换 sn 为 MAC 地址
                const char *sn_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "sn"));
                if (sn_str != NULL) 
                {
                    // 将字符串形式的 MAC 地址转换为字节数组

                    sscanf(sn_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
                }

                // 如果是当前主机的设置
                if(memcmp(get_global_data()->m_mac_uint, mac, 6) == 0)
                {
                    Is_get_host_device_Info = true;
                    get_global_data()->m_device_info.default_counter_time = setting_info.default_counter_time;
                    get_global_data()->m_device_info.overtime_alert_time = setting_info.overtime_alert_time;
                    get_global_data()->m_device_info.is_idel_clock_time = setting_info.is_idel_clock_time;
                    get_global_data()->m_device_info.is_strong_wake_up = setting_info.is_strong_wake_up;
                    get_global_data()->m_device_info.sound_volume = setting_info.sound_volume;
                    get_global_data()->m_device_info.sleep_time = setting_info.sleep_time;
                    memcpy(get_global_data()->m_device_info.language, setting_info.language, sizeof(get_global_data()->m_device_info.language));
                    ESP_LOGI("HTTP", "Save setting to host " MACSTR ", default_counter_time is %d, overtime_alert_time is %d, is_idel_clock_time is %d, is_strong_wake_up is %d, sound_volume is %d, sleep_time is %d, language is %s", 
                    MAC2STR(mac), 
                    get_global_data()->m_device_info.default_counter_time, 
                    get_global_data()->m_device_info.overtime_alert_time, 
                    get_global_data()->m_device_info.is_idel_clock_time, 
                    get_global_data()->m_device_info.is_strong_wake_up,
                    get_global_data()->m_device_info.sound_volume, 
                    get_global_data()->m_device_info.sleep_time,
                    get_global_data()->m_device_info.language);
                }
                // 如果是当前主机下的从机的设置
                else
                {
                    for(int i = 0; i < get_global_data()->m_slave_num; i++)
                    {
                        if(memcmp(get_global_data()->m_slave_info[i].mac, mac, 6) == 0)
                        {
                            Is_get_slave_device_Info[i] = true;
                            get_global_data()->m_slave_info[i].setting.default_counter_time = setting_info.default_counter_time;
                            get_global_data()->m_slave_info[i].setting.overtime_alert_time = setting_info.overtime_alert_time;
                            get_global_data()->m_slave_info[i].setting.is_idel_clock_time = setting_info.is_idel_clock_time;
                            get_global_data()->m_slave_info[i].setting.is_strong_wake_up = setting_info.is_strong_wake_up;  
                            get_global_data()->m_slave_info[i].setting.sound_volume = setting_info.sound_volume;
                            get_global_data()->m_slave_info[i].setting.sleep_time = setting_info.sleep_time;
                            get_global_data()->m_slave_info[i].setting.is_strong_wake_up = setting_info.is_strong_wake_up;
                            memcpy(get_global_data()->m_slave_info[i].setting.language, setting_info.language, sizeof(get_global_data()->m_slave_info[i].setting.language));
                            ESP_LOGI("HTTP", "Save setting to slave " MACSTR ", default_counter_time is %d, overtime_alert_time is %d, is_idel_clock_time is %d, is_strong_wake_up is %d, sound_volume is %d, sleep_time is %d, language is %s", 
                            MAC2STR(mac), 
                            get_global_data()->m_slave_info[i].setting.default_counter_time, 
                            get_global_data()->m_slave_info[i].setting.overtime_alert_time, 
                            get_global_data()->m_slave_info[i].setting.is_idel_clock_time, 
                            get_global_data()->m_slave_info[i].setting.is_strong_wake_up,
                            get_global_data()->m_slave_info[i].setting.sound_volume, 
                            get_global_data()->m_slave_info[i].setting.sleep_time,
                            get_global_data()->m_slave_info[i].setting.language);
                        }
                    }
                }
            }
            
            //如果没有收到主机的信息则解绑自己
            if(!Is_get_host_device_Info)
            {
                ESP_LOGE("HTTP", "Host device info is not get, host " MACSTR " unbind", MAC2STR(get_global_data()->m_mac_uint));
                get_global_data()->need_update_device = true;
                memcpy(get_global_data()->unbind_device_mac[get_global_data()->unbind_device_num], get_global_data()->m_mac_uint, 6);
                get_global_data()->unbind_device_num++;
            }
            
            //如果没有收到从机的信息则解绑对应从机
            for(int i = 0; i < get_global_data()->m_slave_num; i++)
            {
                if(!Is_get_slave_device_Info[i])
                {
                    ESP_LOGE("HTTP", "Slave device info is not get, slave " MACSTR " unbind", MAC2STR(get_global_data()->m_slave_info[i].mac));
                    get_global_data()->need_update_device = true;
                    memcpy(get_global_data()->unbind_device_mac[get_global_data()->unbind_device_num], get_global_data()->m_slave_info[i].mac, 6);
                    get_global_data()->unbind_device_num++;
                }
            }

            get_global_data()->need_update_device_info = true;
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDDEVICE ");
    }
    else if (m_task_struct->task == ADDTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ADDTODO title is %s, todo type is %s",m_task_struct->parament[0],m_task_struct->parament[1]);
    }   
    else if (m_task_struct->task == ADD_ENTER_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ADD_ENTER_FOCUS title is %s, todo type is %s, falling time is %s",m_task_struct->parament[0],m_task_struct->parament[1],m_task_struct->parament[2]);
    }
    else if (m_task_struct->task == ENTER_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ENTER_FOCUS Task %s enter focus",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == OUT_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is OUT_FOCUS Task %s out focus",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == DELETTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is DELETTODO Task %s is deleted",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == BINDDEVICE)
    {
        ESP_LOGI("HTTP", "Successful get response post task is BINDDEVICE is %s", m_task_struct->parament[0]);
    } 

    else if (m_task_struct->task == UNBINDDEVICE)
    {
        ESP_LOGI("HTTP", "Successful get response post task is UNBINDDEVICE is %s", m_task_struct->parament[0]);
    }
    else if (m_task_struct->task == SAVESETTING)
    {
        ESP_LOGI("HTTP", "Successful get response post task is SAVESETTING ");
    }
    else if (m_task_struct->task == SAVEPOWER)
    {
        ESP_LOGI("HTTP", "Successful get response post task is SAVEPOWER ");
    }
    // 释放 JSON 对象
    cJSON_Delete(json);
    *m_http_state = send_success;
}
#endif