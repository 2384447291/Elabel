#include "m_esp_now.h"
#include "m_esp_now_recieve.h"
#include "network.h"

#define ESP_NOW_MSG_DELAY 500 //每一条espnow消息之间的间隔

//为了省电同步的消息由主机发布
#ifdef HOST_TAG
    typedef enum {
        esp_now_send_waiting,
        esp_now_send_processing,
        esp_now_send_recieving,
        esp_now_send_fail,
    }  espnow_send_state;

    espnow_send_state m_espnow_send_state = esp_now_send_waiting;
    bool need_peer = true;
    
    uint8_t wifi_channel;
    wifi_second_chan_t sencond_change = WIFI_SECOND_CHAN_NONE;

    void elabel_state_change()
    {
        if(need_peer) return;
        if(peer_count == 0) return;
    }

    void elabel_task_change()
    {
        if(need_peer) return;
        if(peer_count == 0) return;        
    }

    void elabel_task_and_state_change()
    {
        if(need_peer) return;
        if(peer_count == 0) return;
    }
#endif

#ifdef SLAVE_TAG
    uint8_t state; //是否收到了广播消息,是否配对成功（0表示未配对，1表示配对上了）
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];//主标签的mac地址
#endif

void Add_peer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN])
{
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ESP_NOW, "Malloc peer information fail");
        return;
    }
    // 初始化对等方信息
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = peer_chaneel;                                 //设置频道：将对等方的频道设置为 1。
    peer->ifidx = ESP_IF_WIFI_STA;                                //设置接口索引：将接口索引设置为 ESP_IF_WIFI_STA
    peer->encrypt = false;                                        //设置加密：将加密标志设置为 false，表示不使用加密。
    memcpy(peer->peer_addr, peer_mac, ESP_NOW_ETH_ALEN);   //复制对等方地址：将广播 MAC 地址复制到 peer->peer_addr 中
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);
}

m_espnow_event_send_cb_t dealing_send_evt;
m_espnow_event_recv_cb_t dealing_recieve_evt;

QueueHandle_t m_espnow_send_queue;
QueueHandle_t m_espnow_recieve_queue;

void m_espnow_send(m_espnow_event_send_cb_t* send_esp_now_task)
{
    if (xQueueSend(m_espnow_send_queue, send_esp_now_task, 0) != pdTRUE) 
    {
        ESP_LOGE(ESP_NOW, "m_espnow_send_queue is full.\n");
    }
}

// void send_espnow_data_prepare(m_espnow_event_send_cb_t *send_param)
// {
//     //通过注册信息判断是定向发送还是广播发送
//     send_param->data->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? m_ESPNOW_DATA_BROADCAST : m_ESPNOW_DATA_UNICAST;
//     send_param->data->seq_num = m_espnow_seq[send_param->data->type]++;
//     send_param->data->crc = 0;
//     send_param->data->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)send_param->buffer, send_param->len);
// }

static void m_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	if (status != ESP_NOW_SEND_SUCCESS)
	{
		ESP_LOGE(ESP_NOW, "Send error");
	}
	// else
	// {
	// 	ESP_LOGI(ESP_NOW, "Send success");
	// }
}

static void m_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    m_espnow_event_recv_cb_t recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0) 
    {
        ESP_LOGE(ESP_NOW, "Receive cb arg error");
        return;
    }

    memcpy(recv_cb.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    memcpy(recv_cb.data, data, len);
    recv_cb.data_len = len;

    //塞入队列是做的值拷贝
    if (xQueueSend(m_espnow_recieve_queue, &recv_cb, 0) != pdTRUE) {
        ESP_LOGE(ESP_NOW, "Esp_now receive queue fail");
    }
    else  
    {
        ESP_LOGI(ESP_NOW, "Receive data from "MACSTR", len: %d", MAC2STR(mac_addr), len);
        print_uint8_array(recv_cb.data, recv_cb.data_len);
    }
}

void esp_now_recieve_update(void)
{
    while(uxQueueSpacesAvailable(m_espnow_recieve_queue)<ESPNOW_RECIEVE_QUEUE_SIZE)
    {
        if(xQueueReceive(m_espnow_recieve_queue, &dealing_recieve_evt, 0) == pdTRUE)
        {
            deal_recieve_esp_now_msg(&dealing_recieve_evt);
        }
    }
}


static void esp_now_send_update(void *pvParameter)
{
    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        //处理接收到的信息
        esp_now_recieve_update();

        //如果是主标签需要配对直接打断现在的发送
        #ifdef HOST_TAG
        if(need_peer) 
        {
            vTaskDelay(ESP_NOW_MSG_DELAY/portTICK_PERIOD_MS);

            //断开wifi连接以保证能改wifi_channel，且不会重连
            m_wifi_disconnect();
            ESP_ERROR_CHECK(esp_wifi_set_channel(PEER_CHANEEL, sencond_change));

            uint8_t send_msg[7];
            for (size_t i = 0; i < 6; i++) {
                send_msg[i] = m_broadcast_mac[i];
            }
            send_msg[6] = wifi_channel;
            // 发送广播数据
            esp_err_t err;
            err = esp_now_send(m_broadcast_mac, (uint8_t *)send_msg, sizeof(send_msg));
            if (err!= ESP_OK)
            {
                ESP_LOGE(ESP_NOW, "Send peer broadcat error.\n");
                ESP_LOGE(ESP_NOW, "Error occurred: %s", esp_err_to_name(err)); // 打印错误信息
		    }
            // else
            // {
            //     ESP_LOGI(ESP_NOW, "Send peer broadcat .\n");
            //     print_uint8_array(send_msg,7);
            // } 
        }
        #endif

        // //不在配对模式
        // if(m_espnow_send_state == esp_now_send_waiting)
        // {
        //     if(uxQueueSpacesAvailable(m_espnow_recieve_queue)==0) return;
        //     if(xQueueReceive(m_espnow_send_queue, &dealing_send_evt, 0) == pdTRUE)
        //     {
        //         ESP_LOGI(ESP_NOW, "Process data from "MACSTR"\n.", MAC2STR(dealing_send_evt.mac_addr));
        //     }
        // }
        // else if(m_espnow_send_state == esp_now_send_processing)
        // {

        // }
        // else if(m_espnow_send_state == esp_now_send_recieving)
        // {

        // }
        // else if(m_espnow_send_state == esp_now_send_fail)
    }
}

void m_espnow_init(void)
{
    #ifdef HOST_TAG
    //-----------------------------等待wifi连接上初始化-----------------------------//
    //阻塞等待wifi连接上
    while(get_wifi_status()!=0x02)
    {
        if(get_wifi_status()==0x00) m_wifi_connect();
        uint32_t milliseconds = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if(milliseconds%1000==0) ESP_LOGI(ESP_NOW, "espnow change channel. Wait for wifi connect.\n");
    }

    ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &sencond_change));
    ESP_LOGI(ESP_NOW,"Connect wifi channel is %d\n",wifi_channel);
    //-----------------------------等待wifi连接上了在进行配对-----------------------------//
    #endif



    //-----------------------------初始化esp_now队列-----------------------------//
    m_espnow_recieve_queue = xQueueCreate(ESPNOW_RECIEVE_QUEUE_SIZE, sizeof(m_espnow_event_recv_cb_t));
    if (m_espnow_recieve_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create recieve_queue fail");
        return;
    }

    m_espnow_send_queue = xQueueCreate(ESPNOW_SEND_QUEUE_SIZE, sizeof(m_espnow_event_send_cb_t));
    if (m_espnow_send_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create send_queue fail");
        return;
    }
    //-----------------------------初始化esp_now队列-----------------------------//




    //-----------------------------初始化esp_now,并配置收包发包函数.-----------------------------//
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(m_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(m_espnow_recv_cb) );
    //-----------------------------初始化esp_now,并配置收包发包函数.-----------------------------//




    
   //当且仅当 ESP32 配置为 STA 模式时，允许其进行休眠。
   //-----------------------------节能模式下的操作-----------------------------//
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
   //-----------------------------节能模式下的操作-----------------------------//



   //-----------------------------添加配对设备广播设置0xFF通道-----------------------------//
    Add_peer(PEER_CHANEEL,m_broadcast_mac);
   //-----------------------------添加配对设备广播设置0xFF通道-----------------------------//

    // 启动espnow线程
     xTaskCreate(&esp_now_send_update, "esp_now_task", 4096, NULL, 10, NULL);
}

