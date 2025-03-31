#ifndef ESP_NOW_SLAVE_HPP
#define ESP_NOW_SLAVE_HPP

#include "esp_now_client.hpp"

typedef struct {
    //该次发送的包的唯一id
    uint16_t unique_id = 0;
    //发送的数据长度
    int data_len;
    //发送的数据
    uint8_t data[MAX_EFFECTIVE_DATA_LEN];
    //发送的消息类型
    message_type m_message_type;
} remind_host_t;

class EspNowSlave {
    private:
        TaskHandle_t slave_recieve_update_task_handle = NULL;
        TaskHandle_t slave_send_update_task_handle = NULL;
        QueueHandle_t remind_queue = NULL;  // 提醒队列
        bool is_host_connected = false;
    public:   
        uint8_t host_mac[ESP_NOW_ETH_ALEN];
        uint8_t host_channel;
        bool need_send_data = false;
        remind_host_t remind_host;

        char username[100];
        void init();
        void deinit();

        bool get_host_status()
        {
            return is_host_connected;
        }
        void set_host_status(bool status)
        {
            is_host_connected = status;
        }

        static EspNowSlave* Instance() {
            static EspNowSlave instance;
            return &instance;
        }

        // 上一次收到包的时间
        TickType_t last_recv_heart_time = 0;  
        void slave_espnow_http_get_todo_list();
        void slave_espnow_http_bind_host_request();
        void check_host_status();

        // 添加新的remind_slave到队列
        bool add_remind_to_queue(const remind_host_t& remind) {
            if (xQueueSend(remind_queue, &remind, 0) != pdTRUE) {
                ESP_LOGW(ESP_NOW, "Remind queue is full!");
                return false;
            }
            else
            {
                uint16_t queue_length = (uint16_t)uxQueueMessagesWaiting(remind_queue);
                (void)queue_length;
                ESP_LOGI(ESP_NOW, "Add remind to queue, current queue length: %d", queue_length);
                return true;
            }
        }

        // 获取当前正在处理的remind_slave
        remind_host_t* get_current_remind() {
            remind_host_t* current_remind = nullptr;
            if (xQueuePeek(remind_queue, &current_remind, 0) == pdTRUE) {
                return current_remind;
            }
            return nullptr;
        }

        // 完成当前remind_slave的处理
        void finish_current_remind() {
            remind_host_t temp;
            if (xQueueReceive(remind_queue, &temp, 0) == pdTRUE) {
                // 成功删除队列头部的数据
                ESP_LOGI(ESP_NOW, "Finished processing remind_host, removed from queue");
            }
        }
};

#endif