#include "m_mqtt.h"
#include "http.h"
#include "global_message.h"
#include "mqtt_recieve.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

bool is_mqtt_init = false;

Mqtt_order m_mqtt_order = No_order;

char rev_topic[256] = "service/to/firmware/";
char rev_topic_2[256] = "service/to/client/";
char send_topic[256] = "firmware/to/service/";

char* get_send_topic(void)
{
    return send_topic;
}

//--------------------------------------mqtt中控使用的参数--------------------------------------//
esp_mqtt_client_handle_t mqtt_client;              //mqtt客户端句柄
esp_mqtt_client_handle_t* get_mqtt_client(void)
{
    return &mqtt_client;
}
//--------------------------------------mqtt中控使用的参数--------------------------------------//
// 定义消息队列
static QueueHandle_t mqtt_message_queue = NULL;
#define MQTT_QUEUE_SIZE 10
#define MQTT_QUEUE_ITEM_SIZE sizeof(char*)

// MQTT消息处理线程
static void mqtt_message_handler_task(void *pvParameters)
{
    char *message = NULL;
    while(1) {
        if(xQueueReceive(mqtt_message_queue, &message, portMAX_DELAY) == pdTRUE) {
            if(message != NULL) {
                solve_message(message);
                free(message);
            }
        }
    }
}

// 修改MQTT事件处理函数
void mqtt_event_fun(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(MQTT_TAG,"%s,%d\r\n",event_base,(int)event_id);
    if(event_id==MQTT_EVENT_CONNECTED){            
        esp_mqtt_client_subscribe(mqtt_client,rev_topic,1);    //订阅主题
        esp_mqtt_client_subscribe(mqtt_client,rev_topic_2,1);    
        ESP_LOGI(MQTT_TAG,"success connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DISCONNECTED){        //断开MQTT服务器连接
        ESP_LOGE(MQTT_TAG,"lose connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DATA){                //收到订阅信息
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t )event_data;   //强转获取存放订阅信息的参数
        // ESP_LOGI(MQTT_TAG,"receive data : %.*s from %.*s\r\n",event->data_len,event->data,event->topic_len,event->topic);
        if(event->topic_len == 0 || event->data_len == 0) return;
        
        // 分配内存并复制消息
        char *message = malloc(event->data_len + 1);
        if(message == NULL) {
            ESP_LOGE(MQTT_TAG, "Failed to allocate memory for message");
            return;
        }
        memcpy(message, event->data, event->data_len);
        message[event->data_len] = '\0';
        
        // 将消息发送到队列
        if(xQueueSend(mqtt_message_queue, &message, 0) != pdTRUE) {
            ESP_LOGE(MQTT_TAG, "Failed to send message to queue");
            free(message);
        }
    }
}

void mqtt_client_init(void)
{
    if(is_mqtt_init) return;
    is_mqtt_init = true;
    
    // 创建消息队列
    mqtt_message_queue = xQueueCreate(MQTT_QUEUE_SIZE, MQTT_QUEUE_ITEM_SIZE);
    if(mqtt_message_queue == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to create message queue");
        return;
    }
    
    strcat(rev_topic,get_global_data()->m_mac_str);
    strcat(rev_topic_2,get_global_data()->m_usertoken);
    strcat(send_topic,get_global_data()->m_mac_str);

    esp_mqtt_client_config_t emcct = {
        .broker.address.uri="mqtt://120.77.1.151",   //MQTT服务器的uri
        .broker.address.port=1883,                   //MQTT服务器的端口
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .network.disable_auto_reconnect = false, // 自动重连
        .session.keepalive = 60,
        .session.disable_keepalive = false,
    };
    mqtt_client = esp_mqtt_client_init(&emcct);           //初始化MQTT客户端获取句柄
    if(!mqtt_client)  ESP_LOGI(MQTT_TAG,"mqtt init error!\r\n");
    
    //注册MQTT事件处理函数
    if(esp_mqtt_client_register_event(mqtt_client,ESP_EVENT_ANY_ID,mqtt_event_fun,NULL)!=ESP_OK)  ESP_LOGI(MQTT_TAG,"mqtt register error!\r\n");
 
    //开启MQTT客户端
    if(esp_mqtt_client_start(mqtt_client) != ESP_OK)  ESP_LOGI(MQTT_TAG,"mqtt start errpr!\r\n");
    
    // 创建消息处理线程
    xTaskCreate(mqtt_message_handler_task, "mqtt_handler", 4096, NULL, 0, NULL);
}