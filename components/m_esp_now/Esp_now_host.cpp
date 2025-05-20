#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_time.h"
#include "codec.hpp"
#include "http.h"

static esp_err_t Host_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;
    //------------------------------------------------睡眠请求------------------------------------------------//
    if(m_message_type == Slave2Host_Sleep_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Sleep_Request_Http from " MACSTR, MAC2STR(src_addr));
        if(set_sleep(src_addr, true) == 2)
        {
            ESP_LOGI(ESP_NOW, "" MACSTR" Sleep", MAC2STR(src_addr));
        }
    }   
    else if(m_message_type == Slave2Host_Wakeup_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Wakeup_Request_Http from " MACSTR, MAC2STR(src_addr));
        if(set_sleep(src_addr, false) == 2)
        {
            ESP_LOGI(ESP_NOW, "" MACSTR" Wake up", MAC2STR(src_addr));
        }
    }
    //------------------------------------------------睡眠请求------------------------------------------------//

    //------------------------------------------------所有的非睡眠非睡眠同步请求都可视为唤醒------------------------------------------------//
    if(m_message_type != Slave2Host_Sleep_Request_Http && m_message_type != Slave2Host_Wakeup_Request_Http && m_message_type!= Slave2Host_Synchronous_Request_Http)
    {
        if(set_sleep(src_addr, false) == 2)
        {
            ESP_LOGI(ESP_NOW, "" MACSTR" Wake up", MAC2STR(src_addr));
        }
        
    }
    //------------------------------------------------所有的非睡眠非睡眠同步请求都可视为唤醒------------------------------------------------//

    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//
    if(m_message_type == Test_Start_Request_Slave2Host)
    {
        ESP_LOGI(ESP_NOW, "Receive Test_Start_Request_Slave2Host from " MACSTR, MAC2STR(src_addr));
        EspNowClient::Instance()->test_connecting_send_count = 0;
        espnow_add_peer(src_addr, NULL);
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
        espnow_del_peer(src_addr);
        ESP_LOGI(ESP_NOW, "Receive Test_Stop_Request_Slave2Host from " MACSTR" , send count: %d", MAC2STR(src_addr),EspNowClient::Instance()->test_connecting_send_count);
    }
    else if(m_message_type == default_message_type)
    {
        EspNowClient::Instance()->test_connecting_send_count++;
    }
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//


    //------------------------------------------------解决从机需求------------------------------------------------//
    if(m_message_type == Slave2Host_Bind_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Bind_Request_Http from " MACSTR, MAC2STR(src_addr));
        //添加从机到主机列表
        EspNowHost::Instance()->Add_new_slave(src_addr);
        //告知后端
        http_bind_device(true,src_addr);
        http_save_setting(true,"0050101080",src_addr);
        http_save_power(true,-1,src_addr);
    }
    else if(m_message_type == Slave2Host_Get_Time_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_Time_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->Mqtt_send_time(src_addr);
    }
    else if(m_message_type == Slave2Host_Get_Device_Info_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Get_Device_Info_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->Mqtt_send_device_info(src_addr);
    }
    else if(m_message_type == Slave2Host_UpdateTaskList_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_UpdateTaskList_Request_Http from " MACSTR, MAC2STR(src_addr));
        //如果正在focus则传递focus消息
        if(get_global_data()->m_focus_state->is_focus == 1)
        {
            TodoItem* todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
            focus_message_t focus_message = pack_focus_message(todo->taskType, todo->fallTiming, todo->startTime, get_global_data()->m_focus_state->focus_task_id, todo->title);
            EspNowHost::Instance()->Mqtt_enter_focus(focus_message, src_addr);
        }
        //如果没有focus则发送task更新
        else
        {
            EspNowHost::Instance()->Mqtt_send_task_list(src_addr);
        }
    }
    else if(m_message_type == Slave2Host_Enter_Focus_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Enter_Focus_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->http_response_enter_focus(data_ptr, size);
    }
    else if(m_message_type == Slave2Host_Out_Focus_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Out_Focus_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->http_response_out_focus(data_ptr, size);
    }
    else if(m_message_type == Slave2Host_Synchronous_Request_Http)
    {
        //如果正在focus则传递focus消息
        if(get_global_data()->m_focus_state->is_focus == 1)
        {
            TodoItem* todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
            focus_message_t focus_message = pack_focus_message(todo->taskType, todo->fallTiming, todo->startTime, get_global_data()->m_focus_state->focus_task_id, todo->title);
            EspNowHost::Instance()->Mqtt_enter_focus(focus_message, src_addr);
        }
        //如果没有则发送时间戳同步
        else
        {
            EspNowHost::Instance()->Mqtt_send_time(src_addr);
        }
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Synchronous_Request_Http from " MACSTR, MAC2STR(src_addr));
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
    }

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Host_handle);

    xTaskCreate(esp_now_send_update, "esp_now_host_send_update_task", 4096, NULL, 0, &host_send_update_task_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(EspNowClient::Instance()->m_role == default_role)
    {
        ESP_LOGE(ESP_NOW, "EspNowHost deinit failed, role is default");
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
        http_add_enter_focus((char*)"Record Task",(char*)"3",focus_message.fallTiming,false);
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
    uint8_t temp_data[4];
    //如果该地址属于该机器，则发送设备信息
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        if(Same_mac(get_global_data()->m_slave_info[i].mac, slave_mac))
        {
            temp_data[0] = get_global_data()->m_slave_info[i].setting.default_counter_time;
            temp_data[1] = get_global_data()->m_slave_info[i].setting.overtime_alert_time;
            temp_data[2] = get_global_data()->m_slave_info[i].setting.is_idel_clock_time;
            temp_data[3] = get_global_data()->m_slave_info[i].setting.sound_volume;
            send_message_ack(temp_data, 4, Host2Slave_Device_Info_Control_Mqtt, slave_mac);
            return;
        }
    }
    ESP_LOGE(ESP_NOW, "Slave " MACSTR " not found", MAC2STR(slave_mac));
}

void EspNowHost::Mqtt_send_time(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
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
    send_message_ack(temp_data, 8, Host2Slave_Get_Time_Control_Mqtt, slave_mac);
    ESP_LOGI(ESP_NOW, "Send Host2Slave_Get_Time_Control_Mqtt to " MACSTR " , time: %lld", MAC2STR(slave_mac), time);
}

void EspNowHost::Mqtt_enter_focus(focus_message_t focus_message, const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
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
            send_message_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, get_global_data()->m_slave_info[i].mac);
        }
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Send Host2Slave_Enter_Focus_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
        send_message_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, slave_mac);
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
            send_message_ack(&temp_data, 1, Host2Slave_UpdateTaskList_Control_Mqtt, get_global_data()->m_slave_info[i].mac);
        }
    }
    else
    {
        ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
        send_message_ack(&temp_data, 1, Host2Slave_UpdateTaskList_Control_Mqtt, slave_mac);
    }
}

