#include "esp_now_remind.hpp"

void esp_now_remind_host::update()
{
    switch (send_state) 
    {
        case waiting:
            if(need_pinning)
            {
                send_state = pinning;
                //清空在线从机
                Online_slave_mac.clear();
                pinning_start_time = xTaskGetTickCount();
                pin_message_unique_id = esp_random() & 0xFFFF;
            }
            else
            {
                send_state = target_sending;
                Online_slave_mac = Target_slave_mac;
            }  
            break;
        case pinning:
            // 检查是否超时或者所有从机都在线
            if ((xTaskGetTickCount() - pinning_start_time >= pdMS_TO_TICKS(PINNING_TIMEOUT_MS)) || 
                (Online_slave_mac.count == Target_slave_mac.count)) 
            {
                // 打印pinning情况
                Print_pin_situation();                   
                // 切换到target_sending状态
                send_state = target_sending;
            } 
            else 
            {
                // 继续执行pin操作
                uint8_t broadcast_data = 0;
                EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC, &broadcast_data, 1, Wakeup_Control_Host2Slave, true, pin_message_unique_id);
            }
            break;
        case target_sending:
            //发送实际消息
            EspNowClient::Instance()->send_esp_now_message(BROADCAST_MAC,data,data_len,m_message_type,true,unique_id);
            break;
    }
}

// 获取在线的从机
void esp_now_remind_host::get_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    if(send_state != pinning) return;
    //检测是否在target列表中
    if(Target_slave_mac.exists(slave_mac))
    {
        //插入到online列表中
        if(Online_slave_mac.insert(slave_mac))
        {   
            // ESP_LOGI(ESP_NOW, "Added slave " MACSTR " to online list.\n", MAC2STR(slave_mac));
        }
    }
    else
    {
        ESP_LOGE(ESP_NOW, "Slave " MACSTR " is not in target list", MAC2STR(slave_mac));
    }
}

// 获取从机反馈,2表示完成，1表示成功，0表示失败
bool esp_now_remind_host::get_feedback_from_online_slave(uint8_t slave_mac[ESP_NOW_ETH_ALEN])
{
    if(send_state != target_sending) return false;
    if(Online_slave_mac.removeByMac(slave_mac))
    {
        // ESP_LOGI(ESP_NOW, "Removed slave " MACSTR " from online list.\n", MAC2STR(slave_mac));
        if(Online_slave_mac.count == 0)
        {
            send_state = waiting;
            return true;
        }
    }

    return false;
}

void esp_now_remind_host::Print_pin_situation()
{
    if(send_state != pinning) return;
    if (Online_slave_mac.count != Target_slave_mac.count) 
    {
        ESP_LOGE(ESP_NOW, "Pinning timeout!");
        // 打印在线从机
        ESP_LOGE(ESP_NOW, "Online slaves (%d):", Online_slave_mac.count);
        for(size_t i = 0; i < Online_slave_mac.count; i++) {
            ESP_LOGE(ESP_NOW, "  - " MACSTR "", MAC2STR(Online_slave_mac.bytes[i]));
        }
        
        // 找出未回复的从机
        MacAddress offline_slaves;
        for(size_t i = 0; i < Target_slave_mac.count; i++) 
        {   
            if(!Online_slave_mac.exists(Target_slave_mac.bytes[i])) {
                offline_slaves.insert(Target_slave_mac.bytes[i]);
            }
        }
        // 打印未回复的从机
        ESP_LOGE(ESP_NOW, "Offline slaves (%d):", offline_slaves.count);
        for(size_t i = 0; i < offline_slaves.count; i++) {
            ESP_LOGE(ESP_NOW, "  - " MACSTR "", MAC2STR(offline_slaves.bytes[i]));
        }
    } 
    else 
    {
        ESP_LOGI(ESP_NOW, "Pinning finished: All slaves online");
    }
}