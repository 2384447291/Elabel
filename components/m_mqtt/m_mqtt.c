#include "m_mqtt.h"
#include "http.h"
#include "global_message.h"
#include "mqtt_recieve.h"
#include "cJSON.h"

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
// 定义一个静态缓冲区来存储接收的数据
static char *mqtt_response_buffer = NULL;
static int mqtt_response_buffer_len = 0;

//MQTT事件处理函数
void mqtt_event_fun(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(MQTT_TAG,"%s,%d\r\n",event_base,event_id);
    if(event_id==MQTT_EVENT_CONNECTED){            
        esp_mqtt_client_subscribe(mqtt_client,rev_topic,1);    //订阅主题
        esp_mqtt_client_subscribe(mqtt_client,rev_topic_2,1);    
        ESP_LOGI(MQTT_TAG,"success connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DISCONNECTED){        //断开MQTT服务器连接
        ESP_LOGE(MQTT_TAG,"lose connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DATA){                //收到订阅信息
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t )event_data;   //强转获取存放订阅信息的参数
        ESP_LOGI(MQTT_TAG,"receive data : %.*s from %.*s\r\n",event->data_len,event->data,event->topic_len,event->topic);
        if(event->topic_len == 0 || event->data_len == 0) return;
        {
            mqtt_response_buffer = realloc(mqtt_response_buffer, mqtt_response_buffer_len + event->data_len + 1);
            if (mqtt_response_buffer == NULL) {
                ESP_LOGE("mqtt", "Failed to allocate memory for mqtt response buffer");
            }
            memcpy(mqtt_response_buffer + mqtt_response_buffer_len, event->data, event->data_len);
            mqtt_response_buffer_len += event->data_len;
            if (mqtt_response_buffer != NULL) 
            {
                solve_message(mqtt_response_buffer);
            }

            free(mqtt_response_buffer); // 释放内存
            mqtt_response_buffer = NULL;
            mqtt_response_buffer_len = 0;
        }
    }
}

void mqtt_client_init(void)
{
    if(is_mqtt_init) return;
    is_mqtt_init = true;
    strcat(rev_topic,get_global_data()->m_mac_str);
    strcat(rev_topic_2,get_global_data()->m_usertoken);
    strcat(send_topic,get_global_data()->m_mac_str);

    esp_mqtt_client_config_t emcct = {
        .uri="mqtt://120.77.1.151",  //MQTT服务器的uri
        .port=1883,                   //MQTT服务器的端口
        .keepalive = 60,              // 保持连接时间
        .disable_clean_session = 0,   // 启用clean session
        .disable_auto_reconnect = 0,  // 启用自动重连
    };
    mqtt_client = esp_mqtt_client_init(&emcct);           //初始化MQTT客户端获取句柄
    if(!mqtt_client)  ESP_LOGI(MQTT_TAG,"mqtt init error!\r\n");
    
    //注册MQTT事件处理函数
    if(esp_mqtt_client_register_event(mqtt_client,ESP_EVENT_ANY_ID,mqtt_event_fun,NULL)!=ESP_OK)  ESP_LOGI(MQTT_TAG,"mqtt register error!\r\n");
 
    //开启MQTT客户端
    if(esp_mqtt_client_start(mqtt_client) != ESP_OK)  ESP_LOGI(MQTT_TAG,"mqtt start errpr!\r\n");

}