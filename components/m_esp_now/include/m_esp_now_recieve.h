#ifndef M_ESP_NOW_RECIEVE
#define M_ESP_NOW_RECIEVE
#include "m_esp_now.h"
#include "global_message.h"
#include "esp_log.h"

#ifdef HOST_TAG
#define MAX_PEER_SLAVE 10
    uint8_t peer_count = 0; //主标签配对上的从标签数目
    uint8_t peer_mac_addr[MAX_PEER_SLAVE][ESP_NOW_ETH_ALEN];//连接上的从标签设备
#endif

#ifdef SLAVE_TAG
     uint8_t host_tag_mac[ESP_NOW_ETH_ALEN];
#endif

uint8_t m_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void deal_recieve_esp_now_msg(m_espnow_event_recv_cb_t* m_deal_recieve)
{
    //如果通道是0xFF，0xFF, 0xFF, 0xFF, 0xFF, 0xFF，表示是广播
    if(memcmp(m_deal_recieve->mac_addr,m_broadcast_mac,6)==0)
    {
        #ifdef HOST_TAG
        uint8_t peer_MAC[6];
        memcpy(peer_MAC,m_deal_recieve->mac_addr,6);

        // 检查 peer_MAC 是否在 peer_mac_addr 中
        for (int i = 0; i < peer_count; i++) 
        {
            if (memcmp(peer_mac_addr[i], peer_MAC, 6) == 0) {
                ESP_LOGE(ESP_NOW,"Peer MAC "MACSTR" already exists!\n",MAC2STR(peer_MAC));
                return;
            }
        }

        // 如果不在数组中，添加到 peer_mac_addr
        if (peer_count < MAX_PEER_SLAVE) 
        {
            memcpy(peer_mac_addr[peer_count], peer_MAC, 6);
            peer_count++;
            ESP_LOGI(ESP_NOW,"MAC "MACSTR" successful peer.\n",MAC2STR(peer_MAC));
        }
        #endif


        #ifdef SLAVE_TAG
        //接收消息
        uint8_t msg_host_MAC[6];
        memcpy(msg_host_MAC,m_deal_recieve->data,6);
        uint8_t msg_wifi_channel = m_deal_recieve->data[6];

        if(memcmp(msg_host_MAC,host_tag_mac,6)!=0)
        {
            memcpy(host_tag_mac,msg_host_MAC,6);
            ESP_LOGI(ESP_NOW,"Host MAC "MACSTR" successful peer.\n",MAC2STR(host_tag_mac));
            Add_peer(msg_wifi_channel,host_tag_mac);
        }

        #endif
    }
}

#endif