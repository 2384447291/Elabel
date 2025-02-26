#include "Esp_now_client.hpp"
#include "global_nvs.h"
#include "http.h"

static void esp_now_recieve_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(10);
        //处理接收到的信息
        while(uxQueueSpacesAvailable(EspNowClient::Instance()->recv_queue)<MAX_RECV_QUEUE_SIZE)
        {
            espnow_message_t  recv_msg;   
            if(xQueueReceive(EspNowClient::Instance()->recv_queue, &recv_msg, 0) == pdTRUE)
            {
                //处理接收到的信息
                // ESP_LOGI(ESP_NOW, "Receive data from " MACSTR ", len: %d", MAC2STR(recv_msg.mac_addr), recv_msg.data_len);
                // EspNowClient::Instance()->print_uint8_array(recv_msg.data, recv_msg.data_len);
                if(recv_msg.data[0] == 0xAB && recv_msg.data[1] == 0xCD && recv_msg.data[recv_msg.data_len-1] == 0xEF)
                {
                    if(recv_msg.data[2] == Bind_Feedback_Slave2Host)
                    {
                        EspNowHost::Instance()->add_slave(recv_msg.mac_addr);
                        ESP_LOGI(ESP_NOW,"Host Get EspNow add slave");
                    }
                    else if(recv_msg.data[2] == Feedback_Slave2Host)
                    {
                        if(recv_msg.data[3] == EspNowHost::Instance()->remind_slave.unique_id)
                        {
                            ESP_LOGI(ESP_NOW,"Host Get EspNow feedback slave");
                            EspNowHost::Instance()->get_feedback_slave(recv_msg.mac_addr);
                        }
                    }
                    else if(recv_msg.data[2] == OutFocus_Control_Slave2Host && recv_msg.data[3] != EspNowHost::Instance()->unique_id)
                    {
                        EspNowHost::Instance()->unique_id = recv_msg.data[3];
                        char sstr[12];
                        int task_id = (recv_msg.data[4] << 24) | (recv_msg.data[5] << 16) | (recv_msg.data[6] << 8) | recv_msg.data[7];
                        ESP_LOGI(ESP_NOW,"Host Get EspNow out focus task_id: %d",task_id);
                        sprintf(sstr, "%d", task_id);
                        TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, task_id);
                        if(chose_todo != NULL)
                        {
                            ESP_LOGI(ESP_NOW,"Host Get EspNow out focus title: %s",chose_todo->title);
                            http_out_focus(sstr,false);
                        }
                    }
                    else if(recv_msg.data[2] == EnterFocus_Control_Slave2Host && recv_msg.data[3] != EspNowHost::Instance()->unique_id)
                    {
                        EspNowHost::Instance()->unique_id = recv_msg.data[3];
                        char sstr[12];
                        int task_id = (recv_msg.data[4] << 24) | (recv_msg.data[5] << 16) | (recv_msg.data[6] << 8) | recv_msg.data[7];
                        int fall_time = (recv_msg.data[8] << 24) | (recv_msg.data[9] << 16) | (recv_msg.data[10] << 8) | recv_msg.data[11];
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
}

static void esp_now_send_update(void *pvParameter)
{
    while(1)
    {
        vTaskDelay(Esp_Now_Send_Interval);
        //如果有要发送的remind消息
        if(EspNowHost::Instance()->remind_slave.is_set)
        {
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, EspNowHost::Instance()->remind_slave.data, EspNowHost::Instance()->remind_slave.data_len);
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
    this->client->m_role = host_role;
    //停止搜索设备
    this->client->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    this->client->Addpeer(0, BROADCAST_MAC);
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
    this->client->m_role = default_role;
    vTaskDelete(host_recieve_update_task_handle);
    vTaskDelete(host_send_update_task_handle);
    host_recieve_update_task_handle = NULL;
    host_send_update_task_handle = NULL;
    this->client->Delpeer(BROADCAST_MAC);
    ESP_LOGI(ESP_NOW, "Host deinit success");
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

   // 存储从机mac到nvs
    uint8_t mac_bytes[MAX_SLAVE_NUM * 6];  // 每个MAC地址6个字节
    memset(mac_bytes, 0, sizeof(mac_bytes));
    for(int i = 0; i < slave_num; i++) {
        memcpy(&mac_bytes[i * 6], slave_mac[i], 6);
    }
    // 存储到NVS
    set_nvs_info_set_slave_mac(slave_num, mac_bytes);

    ESP_LOGI(ESP_NOW, "Add slave " MACSTR " success", MAC2STR(_slave_mac));
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
}

void EspNowHost::send_wakeup_message()
{
    if(slave_num == 0) return;
    uint8_t wifi_channel = 0;
    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

    // 计算数据包总长度：头部(4字节) + MAC(6字节) + 用户名长度
    size_t total_len = 6;
    uint8_t broadcast_data[total_len];
    // 填充数据包头部
    broadcast_data[0] = 0xAB;  // 包头
    broadcast_data[1] = 0xCD;  // 包头
    broadcast_data[2] = Wakeup_Control_Host2Slave;
    broadcast_data[3] = esp_random() & 0xFF;  // 随机生成的唯一ID
    broadcast_data[4] = wifi_channel;  // WiFi通道
    broadcast_data[total_len-1] = 0xEF;  // 包尾

    // 填充remind_slave结构体
    remind_slave.is_set = true;
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = broadcast_data[3];
    remind_slave.slave_num_not_feedback = slave_num;
    for(int i = 0; i < slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], slave_mac[i], ESP_NOW_ETH_ALEN);
    }

    ESP_LOGI(ESP_NOW, "Load send wakeup message");
}


void EspNowHost::send_broadcast_message()
{
    uint8_t wifi_channel = 0;
    wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));

    // 计算数据包总长度：头部(4字节) + 用户名长度
    size_t username_len = strlen(get_global_data()->m_userName);
    size_t total_len = 5 + username_len;
    uint8_t broadcast_data[total_len];
    
    // 填充数据包头部
    broadcast_data[0] = 0xAB;  // 包头
    broadcast_data[1] = 0xCD;  // 包头
    broadcast_data[2] = Bind_Control_Host2Slave;
    broadcast_data[3] = wifi_channel;  // WiFi通道
    broadcast_data[total_len-1] = 0xEF;  // 包尾
    
    // 填充用户名
    memcpy(&broadcast_data[4], get_global_data()->m_userName, username_len);

    // 发送数据
    this->client->send_esp_now_message(BROADCAST_MAC, broadcast_data, total_len);
}

void EspNowHost::send_update_task_list()
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;

    // 计算总长度：头部(4字节) + ID(1字节) + 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
    size_t total_len = 4;  // 包头3字节 + 包尾1字节
    total_len += 1;  // ID号1字节
    total_len += 1;  // 任务数量1字节
    
    // 先计算所有任务的总长度
    for(int i = 0; i < list->size; i++) {
        total_len += 1;  // 每个任务的长度信息占1字节
        total_len += 4;  // 任务ID占4字节
        total_len += strlen(list->items[i].title);  // 任务内容
    }

    uint8_t broadcast_data[total_len];
    
    // 填充数据包头部
    broadcast_data[0] = 0xAB;  // 包头
    broadcast_data[1] = 0xCD;  // 包头
    broadcast_data[2] = UpdateTaskList_Control_Host2Slave;  // 消息类型
    broadcast_data[3] = esp_random() & 0xFF;  // 随机生成的唯一ID
    broadcast_data[4] = list->size;  // 任务数量
    
    // 填充任务数据
    size_t offset = 5;  // 当前写入位置（头部4字节 + ID 1字节）
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
    
    // 添加包尾
    broadcast_data[total_len - 1] = 0xEF;

    // 填充remind_slave结构体
    remind_slave.is_set = true;
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = broadcast_data[3];
    remind_slave.slave_num_not_feedback = slave_num;
    for(int i = 0; i < slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], slave_mac[i], ESP_NOW_ETH_ALEN);
    }

    ESP_LOGI(ESP_NOW, "Load update task list data");
}

void EspNowHost::send_out_focus()
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;

    // 计算总长度：头部(4字节) + ID(1字节) + 任务数量(1字节) + 每个任务(长度1字节 + ID 4字节 + 内容)
    size_t total_len = 4;  // 包头3字节 + 包尾1字节
    total_len += 1;  // ID号1字节
    total_len += 1;  // 任务数量1字节
    
    // 先计算所有任务的总长度
    for(int i = 0; i < list->size; i++) {
        total_len += 1;  // 每个任务的长度信息占1字节
        total_len += 4;  // 任务ID占4字节
        total_len += strlen(list->items[i].title);  // 任务内容
    }

    uint8_t broadcast_data[total_len];
    
    // 填充数据包头部
    broadcast_data[0] = 0xAB;  // 包头
    broadcast_data[1] = 0xCD;  // 包头
    broadcast_data[2] = OutFocus_Control_Host2Slave;  // 消息类型
    broadcast_data[3] = esp_random() & 0xFF;  // 随机生成的唯一ID
    broadcast_data[4] = list->size;  // 任务数量
    
    // 填充任务数据
    size_t offset = 5;  // 当前写入位置（头部4字节 + ID 1字节）
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
    
    // 添加包尾
    broadcast_data[total_len - 1] = 0xEF;

    // 填充remind_slave结构体
    remind_slave.is_set = true;
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = broadcast_data[3];
    remind_slave.slave_num_not_feedback = slave_num;
    for(int i = 0; i < slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], slave_mac[i], ESP_NOW_ETH_ALEN);
    }

    ESP_LOGI(ESP_NOW, "Load out focus data");
}

void EspNowHost::send_enter_focus(TodoItem _todo_item)
{
    size_t title_len = strlen(_todo_item.title);
    // 包头(3) + ID(1) + 任务ID(4) + 标题长度(1) + 标题内容(title_len) + 倒计时时间(4) + 包尾(1)
    size_t total_len = 3 + 1 + 4 + 1 + title_len + 4 + 1;
    uint8_t broadcast_data[total_len];
    size_t offset = 0;
    
    // 填充数据包头部
    broadcast_data[offset++] = 0xAB;  // 包头
    broadcast_data[offset++] = 0xCD;  // 包头
    broadcast_data[offset++] = EnterFocus_Control_Host2Slave;  // 消息类型
    broadcast_data[offset++] = esp_random() & 0xFF;  // 随机生成的唯一ID

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

    // 添加包尾
    broadcast_data[offset] = 0xEF;

    // 填充remind_slave结构体
    remind_slave.is_set = true;
    remind_slave.data_len = total_len;
    memcpy(remind_slave.data, broadcast_data, total_len);
    remind_slave.unique_id = broadcast_data[3];
    remind_slave.slave_num_not_feedback = slave_num;
    for(int i = 0; i < slave_num; i++)
    {
        memcpy(remind_slave.slave_mac_not_feedback[i], slave_mac[i], ESP_NOW_ETH_ALEN);
    }

    ESP_LOGI(ESP_NOW, "Load enter focus data");
}

