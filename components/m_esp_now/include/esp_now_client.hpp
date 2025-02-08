#ifndef ESP_NOW_CLIENT_HPP
#define ESP_NOW_CLIENT_HPP

#include "esp_now.h"
#include "esp_log.h"
#include "network.h"
#include "global_message.h"
#include "freertos/queue.h"

#define ESP_NOW "ESPNOW"
#define PEER_CHANEEL 6
#define MAX_DATA_LEN 100
#define MAX_RECV_QUEUE_SIZE 10

// 定义ESP-NOW消息结构体
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t data[MAX_DATA_LEN];
    int data_len;
} espnow_message_t;

uint8_t m_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

class EspNowClient{
    public:
        void init();
        void Addpeer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN]);
        void send_esp_now_message(uint8_t target_mac[8], uint8_t* data, size_t len);
        QueueHandle_t recv_queue;  // 接收消息队列

        static EspNowClient* Instance() {
            static EspNowClient instance;
            return &instance;
        }
};

void EspNowClient::send_esp_now_message(uint8_t target_mac[8], uint8_t* data, size_t len)
{
    // 发送广播数据
    esp_err_t err;
    err = esp_now_send(target_mac, data, len);
    if (err!= ESP_OK)
    {
        ESP_LOGE(ESP_NOW, "Send peer broadcat error.\n");
        ESP_LOGE(ESP_NOW, "Error occurred: %s", esp_err_to_name(err)); // 打印错误信息
    }
}

 
void EspNowClient::Addpeer(uint8_t peer_chaneel, uint8_t peer_mac[ESP_NOW_ETH_ALEN])
{
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ESP_NOW, "Malloc peer information fail");
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = peer_chaneel;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);
}

static void m_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (mac_addr == NULL || data == NULL || len <= 0) 
    {
        ESP_LOGE(ESP_NOW, "Receive cb arg error");
        return;
    }

    // 创建消息结构体并填充数据
    espnow_message_t msg;
    memcpy(msg.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    memcpy(msg.data, data, len);
    msg.data_len = len;

    // 将消息发送到队列,不等待直接塞入
    if (xQueueSend(EspNowClient::Instance()->recv_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(ESP_NOW, "Receive queue fail");
    }
}

static void m_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	if (status != ESP_NOW_SEND_SUCCESS)
	{
		ESP_LOGE(ESP_NOW, "Send error");
	}
}

void EspNowClient::init(){
    //-----------------------------创建接收队列-----------------------------//
    recv_queue = xQueueCreate(MAX_RECV_QUEUE_SIZE, sizeof(espnow_message_t));
    if (recv_queue == NULL) {
        ESP_LOGE(ESP_NOW, "Create receive queue fail");
        return;
    }
    //-----------------------------创建接收队列-----------------------------//


    //-----------------------------初始化esp_now,并配置收包发包函数.-----------------------------//
    ESP_ERROR_CHECK( esp_now_init());
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
    ESP_ERROR_CHECK(esp_wifi_set_channel(PEER_CHANEEL, WIFI_SECOND_CHAN_NONE));
    this->Addpeer(PEER_CHANEEL, m_broadcast_mac);
   //-----------------------------添加配对设备广播设置0xFF通道-----------------------------//
}

class EspNowHost {
    private:
        EspNowClient* client;
    public:
        uint8_t Wifi_channel = 0xFF;
        uint8_t slave_mac[ESP_NOW_ETH_ALEN][6];
        uint8_t slave_num;

        void init();

        static EspNowHost* Instance() {
            static EspNowHost instance;
            return &instance;
        }
        EspNowHost() {
            client = EspNowClient::Instance();
        }
};
#endif