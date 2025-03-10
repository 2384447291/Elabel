#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_nvs.h"
#include "http.h"

void EspNowHost::Broadcast_bind_message()
{
    send_state = broadcasting;
    uint8_t wifi_channel = 0;
    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

    uint8_t username_len = strlen(get_global_data()->m_userName);
    // 计算数据包总长度：用户名 + 通讯信道
    uint8_t total_len = 1 + username_len;
    uint8_t broadcast_data[total_len];
    
    broadcast_data[0] = wifi_channel;  // WiFi通道
    memcpy(&broadcast_data[1], get_global_data()->m_userName, username_len);// 填充用户名

    // 发送数据
    uint16_t unique_id = esp_random() & 0xFFFF;
    EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, broadcast_data, total_len, Bind_Control_Host2Slave, false, unique_id);
}

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        //处理接收到的信息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_packet_queue)<MAX_RECV_PACKET_QUEUE_SIZE)
        {
            espnow_packet_t recv_packet;   
            if(xQueueReceive(EspNowClient::Instance()->recv_packet_queue, &recv_packet, 0) == pdTRUE)
            {
                //处理接收到的信息
                ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_packet.mac_addr), recv_packet.data_len);
                EspNowClient::Instance()->print_uint8_array(recv_packet.data, recv_packet.data_len);
                if(recv_packet.m_message_type == Bind_Feedback_Slave2Host)
                {
                    EspNowHost::Instance()->add_slave(recv_packet.mac_addr);
                    ESP_LOGI(ESP_NOW,"Host Get EspNow add slave  " MACSTR " success", MAC2STR(recv_packet.mac_addr));
                }
                else if(recv_packet.m_message_type == Feedback_ACK)
                {
                    uint16_t unique_id = recv_packet.data[0] << 8 | recv_packet.data[1];
                    if(EspNowHost::Instance()->send_state == pinning)
                    {
                        if(unique_id == EspNowHost::Instance()->pin_message_unique_id)
                        {
                            EspNowHost::Instance()->add_online_slave(recv_packet.mac_addr);
                        }
                    }
                    else if(EspNowHost::Instance()->send_state == target_sending)
                    {
                        if(unique_id == EspNowHost::Instance()->remind_slave.unique_id)
                        {
                            EspNowHost::Instance()->get_feedback_slave(recv_packet.mac_addr);
                        }
                    }
                }
                else if(recv_packet.m_message_type == OutFocus_Control_Slave2Host)
                {
                    char sstr[12];
                    int task_id = (recv_packet.data[0] << 24) | (recv_packet.data[1] << 16) | (recv_packet.data[2] << 8) | recv_packet.data[3];
                    ESP_LOGI(ESP_NOW,"Host Get EspNow out focus task_id: %d",task_id);
                    sprintf(sstr, "%d", task_id);
                    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, task_id);
                    if(chose_todo != NULL)
                    {
                        ESP_LOGI(ESP_NOW,"Host Get EspNow out focus title: %s",chose_todo->title);
                        http_out_focus(sstr,false);
                    }
                }
                else if(recv_packet.m_message_type == EnterFocus_Control_Slave2Host)
                {
                    char sstr[12];
                    int task_id = (recv_packet.data[0] << 24) | (recv_packet.data[1] << 16) | (recv_packet.data[2] << 8) | recv_packet.data[3];
                    int fall_time = (recv_packet.data[4] << 24) | (recv_packet.data[5] << 16) | (recv_packet.data[6] << 8) | recv_packet.data[7];
                    ESP_LOGI(ESP_NOW,"Host Get EspNow enter focus task_id: %d",task_id);
                    ESP_LOGI(ESP_NOW,"Host Get EspNow enter focus fall_time: %d",fall_time);
                    sprintf(sstr, "%d", task_id);
                    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, task_id);
                    if(chose_todo != NULL)
                    {
                        ESP_LOGI(ESP_NOW,"Host Get EspNow enter focus title: %s",chose_todo->title);
                        http_in_focus(sstr,fall_time,false);
                    }
                }
            }
        }
    }
}


static void esp_now_send_update(void *pvParameter)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(Esp_Now_Send_Interval));
        static uint32_t innerclock = 0;
        innerclock+=Esp_Now_Send_Interval;
        //如果有要发送的remind消息
        if(EspNowHost::Instance()->send_state == target_sending)
        {
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, 
            EspNowHost::Instance()->remind_slave.data, 
            EspNowHost::Instance()->remind_slave.data_len, 
            EspNowHost::Instance()->remind_slave.m_message_type, 
            true, 
            EspNowHost::Instance()->remind_slave.unique_id);
        }
        //进入pin模式后自动扫描2s更新remind_slaver,然后切换成waiting
        else if(EspNowHost::Instance()->send_state == pinning)
        {
            //检查是否超时或者所有从机都在线
            if((xTaskGetTickCount() - EspNowHost::Instance()->pinning_start_time >= EspNowHost::Instance()->PINNING_TIMEOUT) || 
            (EspNowHost::Instance()->online_slave_num >= EspNowHost::Instance()->slave_num))
            {
                ESP_LOGI(ESP_NOW, "Pinning finished: %s", 
                    (EspNowHost::Instance()->online_slave_num >= EspNowHost::Instance()->slave_num) ? "All slaves online" : "Timeout");
                EspNowHost::Instance()->send_state = waiting;
            }

            uint8_t broadcast_data = 0;
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, 
            &broadcast_data, 1, 
            Wakeup_Control_Host2Slave, 
            true, 
            EspNowHost::Instance()->pin_message_unique_id);
        }
        else if(EspNowHost::Instance()->send_state == waiting)
        {
            if(innerclock % 2000 != 0) continue;
            uint8_t wifi_channel = 0;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

            // 发送数据
            uint16_t unique_id = esp_random() & 0xFFFF;
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, &wifi_channel, 1, Heart_Control_Host2Slave, false, unique_id);
        }
    }
}

void EspNowHost::init()
{
    if(host_recieve_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host already init");
        return;
    }
    EspNowClient::Instance()->m_role = host_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    EspNowClient::Instance()->Addpeer(0, BROADCAST_MAC);
    xTaskCreate(esp_now_recieve_update, "esp_now_host_recieve_update_task", 4096, NULL, 1, &host_recieve_update_task_handle);
    xTaskCreate(esp_now_send_update, "esp_now_host_send_update_task", 4096, NULL, 10, &host_send_update_task_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(host_recieve_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host not init");
        return;
    }
    EspNowClient::Instance()->m_role = default_role;
    vTaskDelete(host_recieve_update_task_handle);
    vTaskDelete(host_send_update_task_handle);
    host_recieve_update_task_handle = NULL;
    host_send_update_task_handle = NULL;
    EspNowClient::Instance()->Delpeer(BROADCAST_MAC);
    ESP_LOGI(ESP_NOW, "Host deinit success");
}

void EspNowHost::add_online_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN])
{
    for(int i = 0; i < online_slave_num; i++)
    {
        if(memcmp(online_slave_mac[i], _slave_mac, ESP_NOW_ETH_ALEN) == 0)
        {
            ESP_LOGI(ESP_NOW, "Slave " MACSTR " already exists", MAC2STR(_slave_mac));
            return;
        }
    }
    memcpy(online_slave_mac[online_slave_num], _slave_mac, ESP_NOW_ETH_ALEN);
    online_slave_num++; 
    ESP_LOGI(ESP_NOW, "Add online slave " MACSTR " success", MAC2STR(_slave_mac));
}

void EspNowHost::add_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN])
{
    // 首先检查是否已经存在该MAC地址
    for(int i = 0; i < slave_num; i++)
    {
        if(memcmp(slave_mac[i], _slave_mac, ESP_NOW_ETH_ALEN) == 0)
        {
            ESP_LOGI(ESP_NOW, "Slave " MACSTR " already exists", MAC2STR(_slave_mac));
            return;
        }
    }

    // 检查是否超过最大从机数量限制
    if(slave_num >= MAX_SLAVE_NUM)
    {
        ESP_LOGI(ESP_NOW, "Slave number exceeds limit %d", MAX_SLAVE_NUM);
        return;
    }

    // 添加新的从机MAC地址
    memcpy(slave_mac[slave_num], _slave_mac, ESP_NOW_ETH_ALEN);
    slave_num++;
    ESP_LOGI(ESP_NOW, "Add slave " MACSTR " success", MAC2STR(_slave_mac));
    //每次更新slave列表都要更新nvs
    set_SlaveList2NVS();
}

void EspNowHost::del_slave(uint8_t _slave_mac[ESP_NOW_ETH_ALEN])
{
    for(int i = 0; i < slave_num; i++)
    {
        if(memcmp(slave_mac[i], _slave_mac, ESP_NOW_ETH_ALEN) == 0)
        {
            // 找到匹配的MAC地址,将后面的MAC地址前移
            for(int j = i; j < slave_num - 1; j++)
            {
                memcpy(slave_mac[j], slave_mac[j+1], ESP_NOW_ETH_ALEN);
            }
            slave_num--;
            ESP_LOGI(ESP_NOW, "Delete slave " MACSTR " success", MAC2STR(_slave_mac));
            return;
        }
    }
    ESP_LOGI(ESP_NOW, "Slave " MACSTR " not found", MAC2STR(_slave_mac));
    //每次更新slave列表都要更新nvs
    set_SlaveList2NVS();
}


void EspNowHost::pin_slave()
{
    online_slave_num = 0;
    memset(online_slave_mac, 0, sizeof(online_slave_mac));
    //在大逻辑中pining 在检测到所有设备或者2s 中后会变成waiting
    EspNowHost::Instance()->send_state = pinning;
    //记录开始pinning的时间
    pinning_start_time = xTaskGetTickCount();
    pin_message_unique_id = esp_random() & 0xFFFF;
}


void EspNowHost::send_update_task_list()
{
    if(send_state != waiting) 
    {
        ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
        return;
    }
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;

    // 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
    size_t total_len = 0;  // 包头3字节 + 包尾1字节
    total_len += 1;  // 任务数量1字节
    // 先计算所有任务的总长度
    for(int i = 0; i < list->size; i++) {
        total_len += 1;  // 每个任务的长度信息占1字节
        total_len += 4;  // 任务ID占4字节
        total_len += strlen(list->items[i].title);  // 任务内容
    }

    uint8_t broadcast_data[total_len];
    
    broadcast_data[0] = list->size;  // 任务数量
    
    // 填充任务数据
    size_t offset = 1;  // 当前写入位置（头部4字节 + ID 1字节）
    for(int i = 0; i < list->size; i++) {
        size_t title_len = strlen(list->items[i].title);
        broadcast_data[offset++] = title_len;  // 写入当前任务的长度
        
        // 写入任务ID（4字节）
        broadcast_data[offset++] = (list->items[i].id >> 24) & 0xFF;  // ID高字节
        broadcast_data[offset++] = (list->items[i].id >> 16) & 0xFF;
        broadcast_data[offset++] = (list->items[i].id >> 8) & 0xFF;
        broadcast_data[offset++] = list->items[i].id & 0xFF;  // ID低字节
        
        memcpy(&broadcast_data[offset], list->items[i].title, title_len);  // 写入任务内容
        offset += title_len;
    }

    // 填充remind_slave结构体
    empty_remind_slave();
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
    remind_slave.m_message_type = UpdateTaskList_Control_Host2Slave;
    remind_slave.slave_num_not_feedback = online_slave_num;
    for(int i = 0; i < online_slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
    }
    ESP_LOGI(ESP_NOW, "Load update task list data");
    send_state = target_sending;
}

void EspNowHost::send_out_focus()
{
    if(send_state != waiting) 
    {
        ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
        return;
    }
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;

    // 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
    size_t total_len = 0;  // 包头3字节 + 包尾1字节
    total_len += 1;  // 任务数量1字节
    // 先计算所有任务的总长度
    for(int i = 0; i < list->size; i++) {
        total_len += 1;  // 每个任务的长度信息占1字节
        total_len += 4;  // 任务ID占4字节
        total_len += strlen(list->items[i].title);  // 任务内容
    }

    uint8_t broadcast_data[total_len];
    
    broadcast_data[0] = list->size;  // 任务数量
    
    // 填充任务数据
    size_t offset = 1;  // 当前写入位置（头部4字节 + ID 1字节）
    for(int i = 0; i < list->size; i++) {
        size_t title_len = strlen(list->items[i].title);
        broadcast_data[offset++] = title_len;  // 写入当前任务的长度
        
        // 写入任务ID（4字节）
        broadcast_data[offset++] = (list->items[i].id >> 24) & 0xFF;  // ID高字节
        broadcast_data[offset++] = (list->items[i].id >> 16) & 0xFF;
        broadcast_data[offset++] = (list->items[i].id >> 8) & 0xFF;
        broadcast_data[offset++] = list->items[i].id & 0xFF;  // ID低字节
        
        memcpy(&broadcast_data[offset], list->items[i].title, title_len);  // 写入任务内容
        offset += title_len;
    }

    // 填充remind_slave结构体
    empty_remind_slave();
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
    remind_slave.m_message_type = OutFocus_Control_Host2Slave;
    remind_slave.slave_num_not_feedback = online_slave_num;
    for(int i = 0; i < online_slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
    }
    ESP_LOGI(ESP_NOW, "Load update task list data");
    send_state = target_sending;
}

void EspNowHost::send_enter_focus(TodoItem _todo_item)
{
    if(send_state != waiting) 
    {
        ESP_LOGI(ESP_NOW, "Send update task list failed, send_state is not waiting, is %d", send_state);
        return;
    }

    size_t title_len = strlen(_todo_item.title);
    // 任务ID(4) + 标题长度(1) + 标题内容(title_len) + 倒计时时间(4)
    size_t total_len = 4 + 1 + title_len + 4;
    uint8_t broadcast_data[total_len];
    size_t offset = 0;

    // 填充任务ID (4字节)
    broadcast_data[offset++] = (_todo_item.id >> 24) & 0xFF;  // ID高字节
    broadcast_data[offset++] = (_todo_item.id >> 16) & 0xFF;
    broadcast_data[offset++] = (_todo_item.id >> 8) & 0xFF;
    broadcast_data[offset++] = _todo_item.id & 0xFF;  // ID低字节

    // 填充任务标题长度和内容
    broadcast_data[offset++] = title_len;  // 标题长度(1字节)
    memcpy(&broadcast_data[offset], _todo_item.title, title_len);
    offset += title_len;

    // 填充倒计时时间 (4字节)
    broadcast_data[offset++] = (_todo_item.fallTiming >> 24) & 0xFF;
    broadcast_data[offset++] = (_todo_item.fallTiming >> 16) & 0xFF;
    broadcast_data[offset++] = (_todo_item.fallTiming >> 8) & 0xFF;
    broadcast_data[offset++] = _todo_item.fallTiming & 0xFF;

    // 填充remind_slave结构体
    empty_remind_slave();
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = (uint16_t)(esp_random() & 0xFFFF);
    remind_slave.m_message_type = EnterFocus_Control_Host2Slave;
    remind_slave.slave_num_not_feedback = online_slave_num;
    for(int i = 0; i < online_slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], online_slave_mac[i], ESP_NOW_ETH_ALEN);
    }
    ESP_LOGI(ESP_NOW, "Load update task list data");
    send_state = target_sending;
}

