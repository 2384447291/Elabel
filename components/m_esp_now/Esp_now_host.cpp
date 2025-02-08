#include "esp_now_client.hpp"

void print_uint8_array(uint8_t *array, size_t length) {
    // 创建一个字符串缓冲区以存储打印内容
    char buffer[256]; // 假设最大长度为 256 字节
    size_t offset = 0;

    for (size_t i = 0; i < length; i++) {
        // 将每个元素格式化为十六进制并追加到缓冲区
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", array[i]);
    }

    // 使用 ESP_LOGI 打印整个数组
    ESP_LOGI("data", "Array: %s\n", buffer);
}


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
                ESP_LOGI(ESP_NOW, "Receive data from %s, len: %d", MAC2STR(recv_msg.mac_addr), recv_msg.data_len);
                print_uint8_array(recv_msg.data, recv_msg.data_len);
            }
        }
    }
}

void EspNowHost::init()
{
    this->client->init();
    xTaskCreate(&esp_now_recieve_update, "esp_now_task", 4096, NULL, 10, NULL);
}

