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
        EspNowHost::Instance()->Bind_slave_mac.set_sleep(src_addr, true);
    }   
    else if(m_message_type == Slave2Host_Wakeup_Request_Http)
    {
        ESP_LOGI(ESP_NOW, "Receive Slave2Host_Wakeup_Request_Http from " MACSTR, MAC2STR(src_addr));
        EspNowHost::Instance()->Bind_slave_mac.set_sleep(src_addr, false);
    }
    //------------------------------------------------睡眠请求------------------------------------------------//

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
    //------------------------------------------------解决从机需求------------------------------------------------//


    return ESP_OK;
}



void EspNowHost::init()
{
    //从nvs中来的更新从机列表
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

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Host_handle);
    ESP_LOGI(ESP_NOW, "Host init success");
}

void EspNowHost::deinit()
{

    EspNowClient::Instance()->m_role = default_role;

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
        http_add_enter_focus((char*)"Pure Time Task",(char*)"1",focus_message.focus_time,false);
    }
    else if(focus_message.focus_type == 2)
    {
        char sstr[12];
        sprintf(sstr, "%d", focus_message.focus_id);
        http_in_focus(sstr,focus_message.focus_time,false);
    }
    else if(focus_message.focus_type == 3)
    {
        http_add_enter_focus((char*)"Record Task",(char*)"3",focus_message.focus_time,false);
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

void EspNowHost::Mqtt_update_task_list(const uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    TodoList* list = get_global_data()->m_todo_list;
    if (list == NULL) return;
    
    // 发送所有数据包
    int current_task_index = 0;
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
                // 如果从机处于睡眠状态，则不发送数据包
                if(Bind_slave_mac.slaves[i].is_sleep)
                {
                    ESP_LOGI(ESP_NOW, "Slave " MACSTR " is sleep, skip", MAC2STR(Bind_slave_mac.slaves[i].mac));
                    continue;
                }
                ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(Bind_slave_mac.slaves[i].mac));
                send_message_ack(current_packet, offset, Host2Slave_UpdateTaskList_Control_Mqtt, Bind_slave_mac.slaves[i].mac);
            }
        }
        else
        {
            if(Bind_slave_mac.is_sleep(slave_mac))
            {
                ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to no ack" MACSTR, MAC2STR(slave_mac));
                send_message_no_ack(current_packet, offset, Host2Slave_UpdateTaskList_Control_Mqtt, slave_mac);
            }
            else
            {
                ESP_LOGI(ESP_NOW, "Send Host2Slave_UpdateTaskList_Control_Mqtt to " MACSTR, MAC2STR(slave_mac));
                send_message_ack(current_packet, offset, Host2Slave_UpdateTaskList_Control_Mqtt, slave_mac);
            }
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
        if(Bind_slave_mac.slaves[i].is_sleep)
        {
            ESP_LOGI(ESP_NOW, "Slave " MACSTR " is sleep, skip", MAC2STR(Bind_slave_mac.slaves[i].mac));
            continue;
        }
        send_message_ack(temp_data, temp_data_len, Host2Slave_Enter_Focus_Control_Mqtt, Bind_slave_mac.slaves[i].mac);
    }
}

void EspNowHost::Mqtt_out_focus()
{
    ESP_LOGI(ESP_NOW, "Mqtt_out_focus");
    uint8_t temp_data = 0;
    // 添加remind_slave到队列
    for(int i = 0; i < Bind_slave_mac.count; i++)
    {
        send_message_ack(&temp_data, 1, Host2Slave_Out_Focus_Control_Mqtt, Bind_slave_mac.slaves[i].mac);
    }
}

