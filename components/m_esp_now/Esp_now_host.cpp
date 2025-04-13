#include "Esp_now_client.hpp"
#include "Esp_now_host.hpp"
#include "global_time.h"
#include "codec.hpp"
#include "http.h"

// 添加录音数据发送线程的句柄
static TaskHandle_t recording_send_task_handle = NULL;

// 添加一个辅助函数，用于发送数据包
// static bool send_recording_packet(const uint8_t* data, size_t data_len, uint16_t unique_id, message_type msg_type, const MacAddress& target_mac) {
//     // 填充remind_slave结构体
//     esp_now_remind_host remind_slave;
//     remind_slave.data_len = data_len;
//     memcpy(remind_slave.data, data, data_len);
//     remind_slave.unique_id = unique_id;
//     remind_slave.m_message_type = msg_type;
    
//     // 填充Target_slave_mac
//     remind_slave.Target_slave_mac.clear();
//     remind_slave.Target_slave_mac = target_mac;
    
//     // 清除Online_slave_mac
//     remind_slave.Online_slave_mac.clear();
//     remind_slave.need_pinning = false;
    
//     // 尝试添加remind_slave到队列，如果队列满则等待
//     bool added = false;
//     while (!added) {
//         added = EspNowHost::Instance()->add_remind_to_queue(remind_slave);
//         if (!added) {
//             // 使用较短的延时，让出CPU给其他任务执行
//             vTaskDelay(pdMS_TO_TICKS(10));
//         }
//     }
//     return added;
// }

// 录音数据发送线程函数
static void recording_send_task(void *pvParameter)
{
    // // 获取录音数据大小
    // uint32_t recorded_size = MCodec::Instance()->recorded_size;
    
    // // 计算一共要发多少包，向上取整
    // uint16_t packet_num = (recorded_size + (MAX_EFFECTIVE_DATA_LEN - 2) - 1) / (MAX_EFFECTIVE_DATA_LEN - 2);
    // ESP_LOGI(ESP_NOW, "Recording data size: %d, packet num: %d", recorded_size, packet_num);
    
    // // 首先发送序号为0的包，告知接收方录音数据总大小和包的总数量
    // uint8_t info_packet[MAX_EFFECTIVE_DATA_LEN];
    // info_packet[0] = 0; // 序号高字节为0
    // info_packet[1] = 0; // 序号低字节为0，表示这是信息包
    
    // // 将录音数据总大小和包的总数量编码到信息包中
    // // 使用4字节存储录音数据总大小，2字节存储包的总数量
    // info_packet[2] = (recorded_size >> 24) & 0xFF;
    // info_packet[3] = (recorded_size >> 16) & 0xFF;
    // info_packet[4] = (recorded_size >> 8) & 0xFF;
    // info_packet[5] = recorded_size & 0xFF;
    // info_packet[6] = (packet_num >> 8) & 0xFF;
    // info_packet[7] = packet_num & 0xFF;
    
    // // 发送信息包
    // send_recording_packet(info_packet, 8, esp_random() & 0xFFFF, 
    //                      Host2Slave_UpdateRecording_Control_Mqtt, 
    //                      EspNowHost::Instance()->Bind_slave_mac,true);
    
    // ESP_LOGI(ESP_NOW, "Sent info packet: recorded_size=%d, packet_num=%d", recorded_size, packet_num);

    // //等待第一条消息收到回复
    // while(true)
    // {
    //     //如果取不到remind_slave，或者当前状态不是waiting，则退出
    //     if(!EspNowHost::Instance()->get_current_remind(&EspNowHost::Instance()->current_remind) && EspNowHost::Instance()->send_state == waiting)
    //     {
    //         break;
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

    // MacAddress pin_slave_mac = EspNowHost::Instance()->last_pin_slave_mac;
    // EspNowHost::Instance()->last_pin_slave_mac.print();

    // // 发送所有数据包
    // int current_data_index = 0;
    // for(int i = 1; i <= packet_num; i++) // 从1开始，因为0已经用于信息包
    // {
    //     // 计算当前包的数据大小
    //     int current_packet_size = (i == packet_num) ? 
    //         (recorded_size - current_data_index) : 
    //         (MAX_EFFECTIVE_DATA_LEN - 2);
        
    //     // 准备当前数据包
    //     uint8_t current_packet[MAX_EFFECTIVE_DATA_LEN];
    //     // 前两个字节存储包序号
    //     current_packet[0] = (i >> 8) & 0xFF;  // 序号高字节
    //     current_packet[1] = i & 0xFF;         // 序号低字节
    //     // 复制实际数据
    //     memcpy(&current_packet[2], &MCodec::Instance()->record_buffer[current_data_index], current_packet_size);

    //     // 发送数据包
    //     send_recording_packet(current_packet, current_packet_size + 2, esp_random() & 0xFFFF, 
    //                         Host2Slave_UpdateRecording_Control_Mqtt, 
    //                         pin_slave_mac,false);

    //     // 更新当前数据索引
    //     current_data_index += current_packet_size;
        
    //     ESP_LOGI(ESP_NOW, "Sent packet %d/%d, size: %d, data_index: %d", i, packet_num, current_packet_size, current_data_index);
    // }
    
    // ESP_LOGI(ESP_NOW, "All recording data sent successfully");
    
    // // 任务完成后删除自身
    // recording_send_task_handle = NULL;
    // vTaskDelete(NULL);
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    for(int i = 0; i < MAX_EFFECTIVE_DATA_LEN; i++)
    {
        temp_data[i] = esp_random() & 0xFF;
    }
    size_t temp_data_len = MAX_EFFECTIVE_DATA_LEN;
    TickType_t start_time = xTaskGetTickCount();
    for(int i = 0; i < 1000; i++)
    {
        for(int i = 0; i < EspNowHost::Instance()->Bind_slave_mac.count; i++)
        {
            EspNowHost::Instance()->send_message(temp_data, temp_data_len, default_message_type, EspNowHost::Instance()->Bind_slave_mac.bytes[i]);
        }
        size_t temp_data_len = MAX_EFFECTIVE_DATA_LEN;
        for(int i = 0; i < EspNowHost::Instance()->Bind_slave_mac.count; i++)
        {
            EspNowHost::Instance()->send_message(temp_data, temp_data_len, default_message_type, EspNowHost::Instance()->Bind_slave_mac.bytes[i]);
        }
    }
    // 任务完成后删除自身
    ESP_LOGI(ESP_NOW, "All recording data sent successfully, time: %d", xTaskGetTickCount() - start_time);
    recording_send_task_handle = NULL;
    vTaskDelete(NULL);
}

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
        espnow_add_peer(src_addr, NULL);
    }
    else if(m_message_type == Test_Stop_Request_Slave2Host)
    {
        uint8_t temp_data[2];
        temp_data[0] = EspNowClient::Instance()->test_connecting_send_count & 0xFF;
        temp_data[1] = (EspNowClient::Instance()->test_connecting_send_count >> 8) & 0xFF;
        esp_err_t ret;
        do{
            ret = EspNowHost::Instance()->send_message(temp_data, 2, Test_Feedback_Host2Slave,src_addr);
        }while(ret!=ESP_OK);
        espnow_del_peer(src_addr);
        ESP_LOGI(ESP_NOW, "Receive Test_Stop_Request_Slave2Host from " MACSTR" , send count: %d", MAC2STR(src_addr),EspNowClient::Instance()->test_connecting_send_count);
    }
    else if(m_message_type == default_message_type)
    {
        EspNowClient::Instance()->test_connecting_send_count++;
    }
    //------------------------------------------------专门用来测试连接的接口------------------------------------------------//

    if(m_message_type == Slave2Host_Bind_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Bind_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->Add_new_slave(src_addr);
    }
    else if(m_message_type == Slave2Host_UpdateTaskList_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_UpdateTaskList_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->Mqtt_update_task_list(src_addr);
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
    return ESP_OK;
}


// 只是用来发送心跳信息
static void esp_now_send_update(void *pvParameter)
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
    if(host_send_update_task_handle != NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host already init");
        return;
    }

    //这些数据是从nvs中来的更新从机列表
    Bind_slave_mac.clear();
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        Bind_slave_mac.insert(get_global_data()->m_slave_mac[i]);
        espnow_add_peer(get_global_data()->m_slave_mac[i], NULL);
    }

    EspNowClient::Instance()->m_role = host_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //添加配对设备广播设置0xFF通道
    espnow_add_peer(ESPNOW_ADDR_BROADCAST, NULL);

    xTaskCreate(esp_now_send_update, "esp_now_host_send_update_task", 4096, NULL, 0, &host_send_update_task_handle);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Host_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{
    if(host_send_update_task_handle == NULL) 
    {
        ESP_LOGI(ESP_NOW, "Host not init");
        return;
    }
    EspNowClient::Instance()->m_role = default_role;
    vTaskDelete(host_send_update_task_handle);
    host_send_update_task_handle = NULL;
    
    espnow_del_peer(ESPNOW_ADDR_BROADCAST);
    for(int i = 0; i < get_global_data()->m_slave_num; i++)
    {
        espnow_del_peer(get_global_data()->m_slave_mac[i]);
    }

    ESP_LOGI(ESP_NOW, "Host deinit success");
}

void EspNowHost::http_response_enter_focus(uint8_t* data, size_t size)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message(data);
    if(focus_message.focus_type == 1)
    {
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        TodoItem todo;
        cleantodoItem(&todo);

        todo.foucs_type = 1;
        todo.fallTiming = focus_message.focus_time;
        todo.id = focus_message.focus_id;
        todo.isFocus = 1;
        todo.startTime = get_unix_time();
        char title[20] = "Pure Time Task";
        todo.title = title;
        
        clean_todo_list(get_global_data()->m_todo_list);
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
        set_task_list_state(firmware_need_update);
    }
    else if(focus_message.focus_type == 2 || focus_message.focus_type == 0)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_in_focus(sstr,focus_message.focus_time,false);
    }
    else if(focus_message.focus_type == 3)
    {
        get_global_data()->m_focus_state->is_focus = 0;
        get_global_data()->m_focus_state->focus_task_id = 0;

        TodoItem todo;
        cleantodoItem(&todo);

        todo.foucs_type = 3;
        todo.fallTiming = focus_message.focus_time;
        todo.id = focus_message.focus_id;
        todo.isFocus = 1;
        todo.startTime = get_unix_time();
        char title[20] = "Record Task";
        todo.title = title;
        
        clean_todo_list(get_global_data()->m_todo_list);
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
        set_task_list_state(firmware_need_update);
    }
}

void EspNowHost::http_response_out_focus(uint8_t* data, size_t size)
{
    // 解析数据
    focus_message_t focus_message = data_to_focus_message(data);
 
    if(focus_message.focus_type == 2 || focus_message.focus_type == 0)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_out_focus(sstr,false);
    }
    //因为这两个bro都没上传到服务器所以不用out_focus
    else if(focus_message.focus_type == 1)
    {
        //模拟http+mqtt的out_focus
        get_global_data()->m_focus_state->is_focus = 2;
        get_global_data()->m_focus_state->focus_task_id = 0;
        http_get_todo_list(false);
    }
    else if(focus_message.focus_type == 3)
    {
        //模拟http+mqtt的out_focus
        get_global_data()->m_focus_state->is_focus = 2;
        get_global_data()->m_focus_state->focus_task_id = 0;
        http_get_todo_list(false);
    } 
}

void EspNowHost::Start_Mqtt_update_recording()
{
    // 检查是否已经有录音发送任务在运行
    if (recording_send_task_handle != NULL) {
        ESP_LOGW(ESP_NOW, "Recording send task already running");
        return;
    }
    
    // 检查是否有录音数据
    if(MCodec::Instance()->recorded_size == 0) 
    {
        ESP_LOGI(ESP_NOW, "No recording data");
        return;
    }
    
    // 创建录音数据发送线程
    xTaskCreate(
        recording_send_task,
        "recording_send_task",
        4096,
        NULL,
        0,
        &recording_send_task_handle
    );
    
    ESP_LOGI(ESP_NOW, "Recording send task created successfully");
}

void EspNowHost::Stop_Mqtt_update_recording()
{
    if(recording_send_task_handle != NULL)
    {
        vTaskDelete(recording_send_task_handle);
        recording_send_task_handle = NULL;
    }
    ESP_LOGI(ESP_NOW, "Recording send task deleted successfully");
}

void EspNowHost::Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;
    
    // 发送所有数据包
    int current_task_index = 0;
    while(current_task_index < list->size) {
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
        current_packet[ESP_NOW_ETH_ALEN] = list->size;  // 任务总数
        current_packet[ESP_NOW_ETH_ALEN + 1] = (current_task_index == 0) ? true : false;  // 第一个包清除之前的数据，其他包不清除

        // 填充任务数据
        size_t offset = 2 + ESP_NOW_ETH_ALEN;  // 当前写入位置（头部2字节 + 目标mac地址）
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

        // 发送数据包
        if(ESPNOW_ADDR_IS_BROADCAST(slave_mac))
        {
            for(int i = 0; i < Bind_slave_mac.count; i++)
            {
                ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(Bind_slave_mac.bytes[i]));
                send_message(current_packet, offset, Host2Slave_UpdateTaskList_Control_Mqtt, Bind_slave_mac.bytes[i]);
            }
        }
        else
        {
            ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
            send_message(current_packet, offset, Host2Slave_UpdateTaskList_Control_Mqtt, slave_mac);
        }

        current_task_index += tasks_in_packet;
    }
}

void EspNowHost::Mqtt_enter_focus(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Mqtt_enter_focus");
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    // 添加remind_slave到队列
    for(int i = 0; i < Bind_slave_mac.count; i++)
    {
        send_message(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, Bind_slave_mac.bytes[i]);
    }
}

void EspNowHost::Mqtt_out_focus()
{
    ESP_LOGI(ESP_NOW, "Mqtt_out_focus");
    uint8_t temp_data = 0;
    // 添加remind_slave到队列
    for(int i = 0; i < Bind_slave_mac.count; i++)
    {
        send_message(&temp_data, 1, Host2Slave_Out_Focus_Control_Mqtt, Bind_slave_mac.bytes[i]);
    }
}

