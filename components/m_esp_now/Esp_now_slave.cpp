#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include "global_nvs.h"
#include "codec.hpp"
static esp_err_t Slave_handle(uint8_t *src_addr, void *data,
                                       size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    uint8_t* data_ptr = (uint8_t*)data;
    message_type m_message_type = (message_type)(data_ptr[0]);
    //读取数据
    data_ptr++;
    size--;
    EspNowSlave::Instance()->last_recv_heart_time = xTaskGetTickCount();
    if(m_message_type == Host2Slave_UpdateTaskList_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_UpdateTaskList_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_todo_list(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Enter_Focus_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_enter_focus(data_ptr, size);
    }
    else if(m_message_type == Host2Slave_Out_Focus_Control_Mqtt)
    {
        ESP_LOGI(ESP_NOW, "Receive Host2Slave_Out_Focus_Control_Mqtt message unique id.");
        EspNowSlave::Instance()->slave_respense_espnow_mqtt_get_out_focus();
    }
    return ESP_OK;
}

void EspNowSlave::init(uint8_t host_mac[ESP_NOW_ETH_ALEN], uint8_t host_channel, char username[100])
{
    //初始化espnowslave参数
    memcpy(this->host_mac, host_mac, ESP_NOW_ETH_ALEN);
    this->host_channel = host_channel;
    memcpy(this->username, username, sizeof(this->username));
    last_recv_heart_time = 0;

    EspNowClient::Instance()->m_role = slave_role;
    //停止搜索设备
    EspNowClient::Instance()->stop_find_channel();
    //设置信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(this->host_channel, WIFI_SECOND_CHAN_NONE));
    //添加配对host
    espnow_add_peer(host_mac, NULL);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, Slave_handle);
    ESP_LOGI(ESP_NOW, "Slave init success");
}

void EspNowSlave::deinit()
{
    EspNowClient::Instance()->m_role = default_role;
    espnow_del_peer(this->host_mac);
    ESP_LOGI(ESP_NOW, "Slave deinit success");
}

void EspNowSlave::suspend_espnow()
{
    
}

void EspNowSlave::resume_espnow()
{

}
void EspNowSlave::slave_send_espnow_http_sleep_request()
{
    ESP_LOGI(ESP_NOW, "Slave send sleep request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_Sleep_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_wakeup_request()
{
    ESP_LOGI(ESP_NOW, "Slave send wakeup request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_Wakeup_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_get_todo_list()
{
    ESP_LOGI(ESP_NOW, "Slave send update task list request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_UpdateTaskList_Request_Http);
}
void EspNowSlave::slave_send_espnow_http_bind_host_request()
{
    ESP_LOGI(ESP_NOW, "Slave send bind host request message");
    uint8_t temp_data = 0;
    send_message(&temp_data, 1, Slave2Host_Bind_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_enter_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send enter focus task request message");
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    send_message(temp_data, temp_data_len, Slave2Host_Enter_Focus_Request_Http);
}

void EspNowSlave::slave_send_espnow_http_out_focus_task(focus_message_t focus_message)
{
    ESP_LOGI(ESP_NOW, "Slave send out focus task request message");
    uint8_t temp_data[MAX_EFFECTIVE_DATA_LEN];
    size_t temp_data_len = 0;
    focus_message_to_data(focus_message, temp_data, temp_data_len);
    send_message(temp_data, temp_data_len, Slave2Host_Out_Focus_Request_Http);
}


void EspNowSlave::slave_respense_espnow_mqtt_get_todo_list(uint8_t* data, size_t size)
{
    EspNowClient::Instance()->print_uint8_array(data, size);
    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;

    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    bool clear_flag = data[ESP_NOW_ETH_ALEN + 1];  // 获取清除标志

    // 如果是第一个包且需要清除之前的数据
    if(clear_flag) {
        clean_todo_list(get_global_data()->m_todo_list);
    }

    // 从数据包中解析任务，注意偏移量需要加上MAC地址的长度 
    size_t offset = ESP_NOW_ETH_ALEN + 2; // MAC地址(6字节) + 头部(2字节)
    while(offset < size) 
    {
        // 读取任务长度
        size_t title_len = data[offset++];
        
        // 读取任务ID（4字节）
        uint32_t task_id = (data[offset] << 24) | 
                          (data[offset + 1] << 16) |
                          (data[offset + 2] << 8) |
                          data[offset + 3];
        offset += 4;

        // 读取任务内容
        char task_title[50];
        memcpy(task_title, &data[offset], title_len);
        task_title[title_len] = '\0';
        offset += title_len;

        TodoItem todo;
        cleantodoItem(&todo);
        todo.id = task_id;
        todo.title = task_title;
        add_or_update_todo_item(get_global_data()->m_todo_list, todo);
    }
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//

    set_task_list_state(firmware_need_update);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_enter_focus(uint8_t* data, size_t size)
{
    //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
    get_global_data()->m_focus_state->is_focus = 0;
    get_global_data()->m_focus_state->focus_task_id = 0;
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    focus_message_t focus_message = data_to_focus_message(data);
    ESP_LOGI(ESP_NOW, "Receive Host2Slave_Enter_Focus_Control_Mqtt message, focus type: %d, focus time: %d", focus_message.focus_type, focus_message.focus_time);

    TodoItem todo;
    cleantodoItem(&todo);

    todo.taskType = focus_message.focus_type;
    todo.fallTiming = focus_message.focus_time;
    todo.id = focus_message.focus_id;
    todo.isFocus = 1;

    if(focus_message.focus_type == 1)
    {
        char title[20] = "Pure Time Task";;
        todo.title = title;
    }
    else if(focus_message.focus_type == 2)
    {
        todo.title = focus_message.task_name;
    }
    else if(focus_message.focus_type == 3)
    {
        char title[20] = "Record Task";
        todo.title = title;
    }
    //-----------------------------------------这个操作类似于http_get_todo_list-----------------------------------------//
    clean_todo_list(get_global_data()->m_todo_list);
    add_or_update_todo_item(get_global_data()->m_todo_list, todo);
    set_task_list_state(firmware_need_update);
}

void EspNowSlave::slave_respense_espnow_mqtt_get_out_focus()
{
    //清零列表
    get_global_data()->m_focus_state->is_focus = 2;
    get_global_data()->m_focus_state->focus_task_id = 0;
    clean_todo_list(get_global_data()->m_todo_list);
    set_task_list_state(firmware_need_update);
    //重新拉一下http_todo_list
    slave_send_espnow_http_get_todo_list();
}


