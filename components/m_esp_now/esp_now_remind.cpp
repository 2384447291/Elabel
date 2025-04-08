#include "esp_now_remind.hpp"
#include "esp_log.h"

espnow_event_status esp_now_remind::check_feedback(MacAddress &Online_slave_mac, uint16_t &unique_id, uint8_t &count)
{
    //count 表示发送次数
    /* retry backoff time (2,4,8,16,32,64,100,100,...)ms */
    feedback_message_t ack_magic;
    uint32_t delay_ms = (count < 6 ? 1 << count : 50) * SEND_DELAY_UNIT_MSECS;
    do {
        vTaskDelay(pdMS_TO_TICKS(SEND_DELAY_UNIT_MSECS));
        while (feedback_queue && xQueueReceive(feedback_queue, &ack_magic, 0) == pdPASS) 
        {
            if (ack_magic.unique_id == unique_id) {
                Online_slave_mac.removeByMac(ack_magic.mac);
                if(Online_slave_mac.count == 0)
                {
                    return ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED;
                }
            }
        }
        delay_ms -= SEND_DELAY_UNIT_MSECS;
    }while (delay_ms > 0);

    return ESPNOW_EVENT_FEEDBACK_TIME_OUT;
}

espnow_event_status esp_now_remind::check_pining(MacAddress& Target_slave_mac, MacAddress& Online_slave_mac, uint16_t &unique_id, uint8_t &count)
{
    //count 表示发送次数
    /* retry backoff time (2,4,8,16,32,64,100,100,...)ms */
    feedback_message_t ack_magic;
    uint32_t delay_ms = (count < 6 ? 1 << count : 50) * SEND_DELAY_UNIT_MSECS;
    do {
        vTaskDelay(pdMS_TO_TICKS(SEND_DELAY_UNIT_MSECS));
        while (feedback_queue && xQueueReceive(feedback_queue, &ack_magic, 0) == pdPASS) 
        {
            if (ack_magic.unique_id == unique_id) 
            {
                if(Target_slave_mac.exists(ack_magic.mac))
                {
                    Online_slave_mac.insert(ack_magic.mac);
                }
                if(Target_slave_mac.count == Online_slave_mac.count)
                {
                    return ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED;
                }
            }
        }
        delay_ms -= SEND_DELAY_UNIT_MSECS;
    }while (delay_ms > 0);

    return ESPNOW_EVENT_FEEDBACK_TIME_OUT;
}



espnow_event_status esp_now_remind::esp_now_send(uint8_t _send_mac[ESP_NOW_ETH_ALEN], uint8_t* _data, size_t _data_len, 
                                                message_type _m_message_type, MacAddress _target_mac, bool _need_pinning)
{
    if(is_running)
    {
        ESP_LOGE(ESP_NOW, "Espnow send is busy");
        return ESPNOW_EVENT_SEND_BUSY;
    }
    is_running = true;
    //---------------------------------------------------------init----------------------------------------------------------------//
    memcpy(send_mac, _send_mac, ESP_NOW_ETH_ALEN);

    memcpy(data, _data, _data_len);

    data_len = _data_len;

    m_message_type = _m_message_type;

    Target_slave_mac = _target_mac;
    Online_slave_mac.clear();

    need_pinning = _need_pinning;
    if(!need_pinning)
    {
        Online_slave_mac = Target_slave_mac;
    }
    //清空feedback_queue
    feedback_message_t temp_msg;
    while (xQueueReceive(feedback_queue, &temp_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 循环直到队列为空或超时
    }
    espnow_event_status ret = ESPNOW_EVENT_DEFAULT;
    //---------------------------------------------------------init----------------------------------------------------------------//



    //---------------------------------------------------------pinning----------------------------------------------------------------//
    if(need_pinning)
    {
        uint16_t pin_unique_id = esp_random()&0xFFFF;
        TickType_t pinning_start_time = xTaskGetTickCount();
        uint8_t pin_count = 0;
        uint8_t pin_data = 0;
        do{
            if(EspNowClient::Instance()->send_esp_now_message(send_mac, &pin_data, 1, Wakeup_Control_Host2Slave, true, pin_unique_id))
            {
                ret = check_pining(Target_slave_mac, Online_slave_mac, pin_unique_id, pin_count);
                if(ret == ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED) break;
                pin_count++;
            }
        }while(xTaskGetTickCount() - pinning_start_time < pdMS_TO_TICKS(PINNING_TIME_MSECS));

        Print_pin_situation();
    }

    if(Online_slave_mac.count == 0)
    {
        ret = ESPNOW_EVENT_PINING_NOBODY_ONLINE;
        is_running = false;
        ESP_LOGE(ESP_NOW, "No slave online");
        return ret;
    }
    else if(Online_slave_mac.count < Target_slave_mac.count)
    {
        ret = ESPNOW_EVENT_PINING_SOMEBODY_ONLINE;
    }
    else
    {
        ret = ESPNOW_EVENT_PINING_ALL_ONLINE;
    }
    //---------------------------------------------------------sending----------------------------------------------------------------//



    //---------------------------------------------------------sending----------------------------------------------------------------//
    uint8_t send_count = 0;
    uint16_t send_unique_id = esp_random()&0xFFFF;
    TickType_t send_start_time = xTaskGetTickCount();
    do{
        if(EspNowClient::Instance()->send_esp_now_message(send_mac, data, data_len, m_message_type, true, send_unique_id))
        {
            ret = check_feedback(Online_slave_mac, send_unique_id, send_count);
            if(ret == ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED)break;
            send_count++;
        }
    }while(xTaskGetTickCount() - send_start_time < pdMS_TO_TICKS(SENDING_TIME_MSECS));

    if(ret == ESPNOW_EVENT_FEEDBACK_ALL_RECEIVED)
    {
        ret = ESPNOW_EVENT_SENDING_SUCCESS;
    }
    else
    {
        ret = ESPNOW_EVENT_SENDING_TIME_OUT;
    }

    is_running = false;

    return ret;
    //---------------------------------------------------------sending----------------------------------------------------------------//
}
