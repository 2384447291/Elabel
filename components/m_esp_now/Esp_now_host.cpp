#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_time.h"
#include "codec.hpp"
#include "global_message.h"
#include "http.h"
#include "ElabelController.hpp"
#include "FocusTaskState.hpp"
#include "global_nvs.h"

static esp_err_t Host_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//
    if(m_message_type == Test_Start_Request_Slave2Host)
    {
        ESP_LOGI(ESP_NOW, "Receive Test_Start_Request_Slave2Host from " MACSTR, MAC2STR(src_addr));
        EspNowClient::Instance()->test_connecting_send_count = 0;
        if(!mac_address_exists(src_addr))
        {
            espnow_add_peer(src_addr, NULL);
        }
    }
    else if(m_message_type == Test_Stop_Request_Slave2Host)
    {
        uint8_t temp_data[2];
        temp_data[0] = EspNowClient::Instance()->test_connecting_send_count & 0xFF;
        temp_data[1] = (EspNowClient::Instance()->test_connecting_send_count >> 8) & 0xFF;
        esp_err_t ret;
        do{
            ret = EspNowHost::Instance()->send_message_ack(temp_data, 2, Test_Feedback_Host2Slave,src_addr);
        }while(ret!=ESP_OK);
        if(!mac_address_exists(src_addr))
        {
            espnow_del_peer(src_addr);
        }
        ESP_LOGI(ESP_NOW, "Receive Test_Stop_Request_Slave2Host from " MACSTR" , send count: %d", MAC2STR(src_addr),EspNowClient::Instance()->test_connecting_send_count);
    }
    else if(m_message_type == default_message_type)
    {
        EspNowClient::Instance()->test_connecting_send_count++;
    }
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//


    //--------------------------------绑定请求--------------------------------//
    if(m_message_type == Slave2Host_Bind_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Bind_Request_Http from " MACSTR, MAC2STR(src_addr));
        //添加从机到主机列表
        EspNowHost::Instance()->Add_new_slave(src_addr);
        //告知后端
        http_bind_device(true,src_addr);
        //从机的基础设置
        //-005:默认计时5分钟
        //-010：超时提示10s间隔
        //-0:不开启默认时钟
        //-000:默认音量0
        //-180:休眠时间180秒
        //-0:表示不开启强提醒
        char language[12] = {0};
        // 如果从机在绑定时上报了 language，则按优先使用从机上报值
        // 数据格式: [0]=长度N, 后随N字节的language字符串
        if(size >= 1 && data_ptr[0] > 0 && (size_t)(1 + data_ptr[0]) <= size && data_ptr[0] < sizeof(language))
        {
            memcpy(language, &data_ptr[1], data_ptr[0]);
            language[data_ptr[0]] = '\0';
            ESP_LOGI(ESP_NOW, "Use slave-reported language on bind: %s", language);
        }
        else
        {
            get_language_nvs_info(language);
            ESP_LOGI(ESP_NOW, "Use host default language on bind: %s", language);
        }
        char setting_str[40];
        sprintf(setting_str, "00501000001800-%s", language);
        http_save_setting(true, setting_str, src_addr);
        http_save_power(true,-1,src_addr);
    }
    //--------------------------------绑定请求--------------------------------//


    //------------------------------------------------睡眠请求------------------------------------------------//
    if(m_message_type == Slave2Host_Sleep_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Sleep_Request_Http from " MACSTR, MAC2STR(src_addr));
            if(set_sleep(src_addr, true) == 2)
            {
                ESP_LOGI(ESP_NOW, "" MACSTR" Sleep", MAC2STR(src_addr));
            }
        }
    }   
    else if(m_message_type == Slave2Host_Wakeup_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Wakeup_Request_Http from " MACSTR, MAC2STR(src_addr));
            if(set_sleep(src_addr, false) == 2)
            {
                ESP_LOGI(ESP_NOW, "" MACSTR" Wake up", MAC2STR(src_addr));
            }
        }
    }

    //所有的非睡眠非睡眠同步请求和测试请求都可视为唤醒
    if(m_message_type != Host2Slave_Bind_Control_Http &&
        m_message_type != Slave2Host_Sleep_Request_Http && m_message_type != Slave2Host_Wakeup_Request_Http && m_message_type!= Slave2Host_Synchronous_Request_Http 
    && m_message_type != Test_Start_Request_Slave2Host && m_message_type != Test_Stop_Request_Slave2Host && m_message_type != default_message_type)
    {
        if(set_sleep(src_addr, false) == 2)
        {
            ESP_LOGI(ESP_NOW, "" MACSTR" Wake up", MAC2STR(src_addr));
        }    
    }

    //------------------------------------------------睡眠请求------------------------------------------------//


    //------------------------------------------------解决从机需求------------------------------------------------//
    if(m_message_type == Slave2Host_Get_Time_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_Time_Request_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->Mqtt_send_time(src_addr);
        }
    }
    else if(m_message_type == Slave2Host_Get_Device_Info_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_Device_Info_Request_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->Mqtt_send_device_info(src_addr);
        }
    }
    else if(m_message_type == Slave2Host_UpdateTaskList_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_UpdateTaskList_Request_Http from " MACSTR, MAC2STR(src_addr));
            //如果正在focus则传递focus消息
            if(ElabelController::Instance()->m_elabelFsm.GetCurrentState() == FocusTaskState::Instance())
            {
                TodoItem* todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->focusing_task_id);
                focus_message_t focus_message = pack_focus_message(todo->taskType, todo->fallTiming, todo->startTime, get_global_data()->focusing_task_id, todo->title);
                EspNowHost::Instance()->Mqtt_enter_focus(focus_message, src_addr);
            }
            //如果没有focus则发送task更新
            else
            {
                EspNowHost::Instance()->Mqtt_send_task_list(src_addr);
            }
        }
    }
    else if(m_message_type == Slave2Host_Enter_Focus_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Enter_Focus_Request_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->http_response_enter_focus(data_ptr, size);
        }
    }
    else if(m_message_type == Slave2Host_Out_Focus_Request_Http)
    {   
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Out_Focus_Request_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->http_response_out_focus(data_ptr, size);
        }
    }
    else if(m_message_type == Slave2Host_Get_Wifi_Info_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_Wifi_Info_Request_Http from " MACSTR, MAC2STR(src_addr));
            int language_length = data_ptr[0];
            char language[language_length + 1];
            memcpy(language, &data_ptr[1], language_length);
            language[language_length] = '\0';
            EspNowHost::Instance()->Mqtt_send_wifi_info(src_addr, language);
        }
    }
    else if(m_message_type == Slave2Host_Get_UserToken_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_UserToken_Request_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->Mqtt_send_usertoken(src_addr);
        }
    }
    else if(m_message_type == Slave2Host_Unbind_Device_Control_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Unbind_Device_Control_Http from " MACSTR, MAC2STR(src_addr));
            EspNowHost::Instance()->Delete_exist_slave(src_addr);
            //通知后端删除从机
            http_unbind_device(true, src_addr);
        }
    }
    else if(m_message_type == Slave2Host_Send_power_message_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            int power_value = (int)data_ptr[0] << 24 | (int)data_ptr[1] << 16 | (int)data_ptr[2] << 8 | (int)data_ptr[3];
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Send_power_message_Http from " MACSTR " , power state: %d", MAC2STR(src_addr), power_value);
            http_save_power(true,power_value,src_addr);
        }
    }
    else if(m_message_type == Slave2Host_Delete_Task_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            int task_id = (int)data_ptr[0] << 24 | (int)data_ptr[1] << 16 | (int)data_ptr[2] << 8 | (int)data_ptr[3];
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Delete_Task_Http from " MACSTR " , task id: %d", MAC2STR(src_addr), task_id);
            char ChosenTaskId_str[10];
            sprintf(ChosenTaskId_str, "%d", task_id);
            http_delet_todo(ChosenTaskId_str, true);
        }
    }
    else if(m_message_type == Slave2Host_Synchronous_Request_Http)
    {
        if(!EspNowHost::Instance()->recieve_from_unbind_device(src_addr))
        {
            //如果正在focus则传递focus消息
            if(ElabelController::Instance()->m_elabelFsm.GetCurrentState() == FocusTaskState::Instance())
            {
                TodoItem* todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->focusing_task_id);
                focus_message_t focus_message = pack_focus_message(todo->taskType, todo->fallTiming, todo->startTime, get_global_data()->focusing_task_id, todo->title);
                EspNowHost::Instance()->Mqtt_enter_focus(focus_message, src_addr, true);
            }
            //如果没有则发送时间戳同步
            else
            {
                EspNowHost::Instance()->Mqtt_send_time(src_addr, false);
            }
            ESP_LOGI(ESP_NOW, "Receive Slave2Host_Synchronous_Request_Http from " MACSTR, MAC2STR(src_addr));
        }
    }
    //------------------------------------------------解决从机需求------------------------------------------------//


    return ESP_OK;
}

void esp_now_send_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(HEART_BEAT_TIME_MSECS));
        if(!EspNowHost::Instance()->is_sending_message)
        {
            EspNowHost::Instance()->send_bind_heartbeat();
        }
    }
}


void EspNowHost::init()
{
    if(EspNowClient::Instance()->m_role == host_role)
    {
        ESP_LOGE(ESP_NOW, "EspNowHost already init, role is host");
        return;
    }
    EspNowClient::Instance()->m_role = host_role;
    //停止搜索设备,保险起见
    EspNowClient::Instance()->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    espnow_add_peer(ESPNOW_ADDR_BROADCAST, NULL);
    //添加各个从机
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        espnow_add_peer(get_global_data()->m_slave_info[i].mac, NULL);
        set_sleep(get_global_data()->m_slave_info[i].mac, true);
    }

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Host_handle);

    xTaskCreate(esp_now_send_update, "esp_now_host_send_update_task", 4096, NULL, 0, &host_send_update_task_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(EspNowClient::Instance()->m_role == default_role)
    {
        // ESP_LOGE(ESP_NOW, "EspNowHost deinit failed, role is default");
        return;
    }

    EspNowClient::Instance()->m_role = default_role;

    // 删除发送心跳绑定包的任务
    vTaskDelete(host_send_update_task_handle);
    host_send_update_task_handle = NULL;

    espnow_del_peer(ESPNOW_ADDR_BROADCAST);
    
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        espnow_del_peer(get_global_data()->m_slave_info[i].mac);
    }

    ESP_LOGI(ESP_NOW, "Host deinit success");
}

void EspNowHost::http_response_enter_focus(uint8_t* data, size_t size)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message(data);
    if(focus_message.focus_type == 1)
    {
        http_add_enter_focus((char*)"Pure Time Task",(char*)"1",focus_message.fallTiming,false);
    }
    else if(focus_message.focus_type == 2)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_in_focus(sstr,focus_message.fallTiming,false);
    }
    else if(focus_message.focus_type == 3)
    {
        http_add_enter_focus(focus_message.task_name,(char*)"3",focus_message.fallTiming,false);
    }
}

void EspNowHost::http_response_out_focus(uint8_t* data, size_t size)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message(data);

    char sstr[12];
    sprintf(sstr, "%d", focus_message.focus_id);
    http_out_focus(sstr,false);
}

void EspNowHost::Mqtt_send_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;
    
    // 发送所有数据包
    int current_task_index = 0;
    if(list->size == 0)
    {
        uint8_t temp_data[2];
        //表示有0个任务
        temp_data[0] = 0;
        //表示要清除
        temp_data[1] = 1;
        send_message_ack(temp_data, 2, Host2Slave_Send_Task_List_Control_Mqtt, slave_mac);
        return;
    }
    while(current_task_index < list->size) 
    {
        // 计算当前包可以包含的任务数量
        size_t current_packet_len = 2;  // 头部2字节（任务总数和清除标志）
        int tasks_in_packet = 0;
        
        // 尝试添加任务，直到再添加一个就会超过最大长度
        while(current_task_index + tasks_in_packet < list->size) {
            size_t next_task_size = 1;  // 任务长度
            next_task_size += 4;  // 任务ID
            next_task_size += strlen(list->items[current_task_index + tasks_in_packet].title);  // 任务内容
            
            // 检查添加下一个任务是否会超过最大长度
            if(current_packet_len + next_task_size > MAX_EFFECTIVE_DATA_LEN) {
                break;
            }
            
            current_packet_len += next_task_size;
            tasks_in_packet++;
        }
        
        // 填充当前包的数据
        uint8_t current_packet[MAX_EFFECTIVE_DATA_LEN];
        current_packet[0] = list->size;  // 任务总数
        current_packet[1] = (current_task_index == 0) ? true : false;  // 第一个包清除之前的数据，其他包不清除

        // 填充任务数据
        size_t offset = 2;  // 当前写入位置（头部2字节 + 目标mac地址）
        for(int i = 0; i < tasks_in_packet; i++) {
            size_t title_len = strlen(list->items[current_task_index + i].title);
            current_packet[offset++] = title_len;  // 写入当前任务的长度
            
            // 写入任务ID（4字节）
            current_packet[offset++] = (list->items[current_task_index + i].id >> 24) & 0xFF;  // ID高字节
            current_packet[offset++] = (list->items[current_task_index + i].id >> 16) & 0xFF;
            current_packet[offset++] = (list->items[current_task_index + i].id >> 8) & 0xFF;
            current_packet[offset++] = list->items[current_task_index + i].id & 0xFF;  // ID低字节
            
            memcpy(&current_packet[offset], list->items[current_task_index + i].title, title_len);  // 写入任务内容
            offset += title_len;
        }

        send_message_ack(current_packet, offset, Host2Slave_Send_Task_List_Control_Mqtt, slave_mac);
        current_task_index += tasks_in_packet;
    }
}   

void EspNowHost::Mqtt_send_device_info(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    uint8_t temp_data[21];
    //如果该地址属于该机器，则发送设备信息
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        if(Same_mac(get_global_data()->m_slave_info[i].mac, slave_mac))
        {
            temp_data[0] = (get_global_data()->m_slave_info[i].setting.default_counter_time >> 8) & 0xFF;
            temp_data[1] = get_global_data()->m_slave_info[i].setting.default_counter_time & 0xFF;

            temp_data[2] = (get_global_data()->m_slave_info[i].setting.overtime_alert_time >> 8) & 0xFF;
            temp_data[3] = get_global_data()->m_slave_info[i].setting.overtime_alert_time & 0xFF;

            temp_data[4] = get_global_data()->m_slave_info[i].setting.is_idel_clock_time;

            temp_data[5] = get_global_data()->m_slave_info[i].setting.sound_volume;
            
            temp_data[6] = (get_global_data()->m_slave_info[i].setting.sleep_time >> 8) & 0xFF;
            temp_data[7] = get_global_data()->m_slave_info[i].setting.sleep_time & 0xFF;

            temp_data[8] = get_global_data()->m_slave_info[i].setting.is_strong_wake_up;
            
            memcpy(&temp_data[9], get_global_data()->m_slave_info[i].setting.language, 12);

            send_message_ack(temp_data, 21, Host2Slave_Device_Info_Control_Mqtt, slave_mac);
            return;
        }
    }
    ESP_LOGE(ESP_NOW, "Slave " MACSTR " not found", MAC2STR(slave_mac));
}

void EspNowHost::Mqtt_send_time(const uint8_t slave_mac[ESP_NOW_ETH_ALEN], bool need_ack)
{
    uint8_t temp_data[8];
    long long time = get_unix_time();
    temp_data[0] = (time >> 56) & 0xFF;
    temp_data[1] = (time >> 48) & 0xFF;
    temp_data[2] = (time >> 40) & 0xFF;
    temp_data[3] = (time >> 32) & 0xFF;
    temp_data[4] = (time >> 24) & 0xFF;
    temp_data[5] = (time >> 16) & 0xFF;
    temp_data[6] = (time >> 8) & 0xFF;
    temp_data[7] = time & 0xFF;
    if(need_ack)
    {
        send_message_ack(temp_data, 8, Host2Slave_Get_Time_Control_Mqtt, slave_mac);
    }
    else
    {
        send_message_no_ack(temp_data, 8, Host2Slave_Get_Time_Control_Mqtt, slave_mac);
    }
    ESP_LOGI(ESP_NOW, "Send Host2Slave_Get_Time_Control_Mqtt to " MACSTR " , time: %lld", MAC2STR(slave_mac), time);
}

void EspNowHost::Mqtt_enter_focus(focus_message_t focus_message, const uint8_t slave_mac[ESP_NOW_ETH_ALEN], bool need_ack)
{
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    // 添加remind_slave到队列
    if(ESPNOW_ADDR_IS_BROADCAST(slave_mac))
    {
        for(int i = 0; i < get_global_data()->m_slave_num; i++)
        {
            if(get_global_data()->m_slave_info[i].is_sleep)
            {
                ESP_LOGI(ESP_NOW, "Slave " MACSTR " is sleep, skip", MAC2STR(get_global_data()->m_slave_info[i].mac));
                continue;
            }
            ESP_LOGI(ESP_NOW, "Send Host2Slave_Enter_Focus_Control_Mqtt to " MACSTR, MAC2STR(get_global_data()->m_slave_info[i].mac));
            esp_err_t ret = send_message_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, get_global_data()->m_slave_info[i].mac);
            if(ret != ESP_OK)
            {
                ESP_LOGE(ESP_NOW, "Broadcast Host2Slave_Enter_Focus_Control_Mqtt to " MACSTR " failed change it to sleepmode", MAC2STR(get_global_data()->m_slave_info[i].mac));
                set_sleep(get_global_data()->m_slave_info[i].mac, true);
            }
        }
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Send Host2Slave_Enter_Focus_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
        if(need_ack)
        {
            send_message_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, slave_mac);
        }
        else
        {
            send_message_no_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, slave_mac);
        }
    }
}

void EspNowHost::Mqtt_out_focus()
{
    uint8_t temp_data = 0;
    // 添加remind_slave到队列   
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        send_message_ack(&temp_data, 1, Host2Slave_Out_Focus_Control_Mqtt, get_global_data()->m_slave_info[i].mac);
    }
}

void EspNowHost::Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    uint8_t temp_data = 0;
    if(ESPNOW_ADDR_IS_BROADCAST(slave_mac))
    {
        for(int i = 0; i < get_global_data()->m_slave_num; i++)
        {
            // 如果从机处于睡眠状态，则不发送数据包
            if(get_global_data()->m_slave_info[i].is_sleep)
            {
                ESP_LOGI(ESP_NOW, "Slave " MACSTR " is sleep, skip", MAC2STR(get_global_data()->m_slave_info[i].mac));
                continue;
            }
            ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(get_global_data()->m_slave_info[i].mac));
            esp_err_t ret = send_message_ack(&temp_data, 1, Host2Slave_UpdateTaskList_Control_Mqtt, get_global_data()->m_slave_info[i].mac);
            if(ret != ESP_OK)
            {
                ESP_LOGE(ESP_NOW, "Broadcast Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR " failed change it to sleepmode", MAC2STR(get_global_data()->m_slave_info[i].mac));
                set_sleep(get_global_data()->m_slave_info[i].mac, true);
            }
        }
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
        send_message_ack(&temp_data, 1, Host2Slave_UpdateTaskList_Control_Mqtt, slave_mac);
    }
}

void EspNowHost::Mqtt_send_usertoken(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    uint8_t user_token_len = strlen(get_global_data()->m_usertoken);
    uint8_t temp_data[user_token_len + 1];
    temp_data[0] = user_token_len;
    memcpy(temp_data + 1, get_global_data()->m_usertoken, user_token_len);
    send_message_ack(temp_data, user_token_len + 1, Host2Slave_Get_UserToken_Control_Mqtt, slave_mac);
}

void EspNowHost::Mqtt_send_wifi_info(const uint8_t slave_mac[ESP_NOW_ETH_ALEN], char* language)
{
    uint8_t wifi_name_len = strlen(get_global_data()->m_wifi_ssid);
    uint8_t wifi_password_len = strlen(get_global_data()->m_wifi_password);

    //暂存之前firmware信息免得被http_get_latest_version覆盖
    char temp_version[100] = {0};
    char temp_deviceModel[100] = {0};
    char temp_newest_firmware_url[100] = {0};
    char temp_createTime[100] = {0};
    char temp_content[1024] = {0};
    
    memcpy(temp_version, get_global_data()->m_version, sizeof(temp_version));
    memcpy(temp_deviceModel, get_global_data()->m_deviceModel, sizeof(temp_deviceModel));
    memcpy(temp_newest_firmware_url, get_global_data()->m_newest_firmware_url, sizeof(temp_newest_firmware_url));
    memcpy(temp_createTime, get_global_data()->m_createTime, sizeof(temp_createTime));
    memcpy(temp_content, get_global_data()->m_content, sizeof(temp_content));
    
    bool get_firmware_need_update = http_get_latest_version(DEVICE_MODEL, language, true);
    uint8_t firmware_version_len = 0;
    if(get_firmware_need_update)
    {
        firmware_version_len = strlen(get_global_data()->m_version);
    }
    
    uint8_t total_len = wifi_name_len + wifi_password_len + firmware_version_len + 3;
    uint8_t temp_data[total_len];

    temp_data[0] = wifi_name_len;
    memcpy(1 + temp_data, get_global_data()->m_wifi_ssid, wifi_name_len);

    temp_data[1 + wifi_name_len] = wifi_password_len;
    memcpy(2 + temp_data + wifi_name_len, get_global_data()->m_wifi_password, wifi_password_len);

    temp_data[2 + wifi_name_len + wifi_password_len] = firmware_version_len;
    memcpy(3 + temp_data + wifi_name_len + wifi_password_len, get_global_data()->m_version, firmware_version_len);

    //恢复之前的firmware信息
    memcpy(get_global_data()->m_version, temp_version, sizeof(temp_version));
    memcpy(get_global_data()->m_deviceModel, temp_deviceModel, sizeof(temp_deviceModel));
    memcpy(get_global_data()->m_newest_firmware_url, temp_newest_firmware_url, sizeof(temp_newest_firmware_url));
    memcpy(get_global_data()->m_createTime, temp_createTime, sizeof(temp_createTime));
    memcpy(get_global_data()->m_content, temp_content, sizeof(temp_content));

    send_message_ack(temp_data, total_len, Host2Slave_Get_Wifi_Info_Control_Mqtt, slave_mac);
}

void EspNowHost::Mqtt_send_unbind_device(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    bool is_peer_exist = esp_now_is_peer_exist(slave_mac);
    if(!is_peer_exist) espnow_add_peer(slave_mac, NULL);
    uint8_t temp_data = 0;
    ESP_LOGI(ESP_NOW, "Send Host2Slave_Unbind_Device_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
    send_message_ack(&temp_data, 1, Host2Slave_Unbind_Device_Control_Mqtt, slave_mac);
    if(!is_peer_exist) espnow_del_peer(slave_mac);
}

bool EspNowHost::recieve_from_unbind_device(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    if(mac_address_exists(slave_mac))
    {
        return false;
    }
    else
    {
        //如果从机不存在，则删除从机
        Mqtt_send_unbind_device(slave_mac);
        http_unbind_device(true, slave_mac);
        ESP_LOGE(ESP_NOW, "Slave " MACSTR " does not exist", MAC2STR(slave_mac));
        return true;
    }
}
