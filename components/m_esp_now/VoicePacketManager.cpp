#include "VoicePacketManager.hpp"
#include "global_message.h"
#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"
#include "codec.hpp"

static void recording_send_task(void *pvParameter)
{
    while(!VoicePacketManager::Instance()->need_stop_recording_send_task)
    {
        espnow_addr_t temp_dest_addr;
        if(get_global_data()->m_is_host)memcpy(temp_dest_addr, ESPNOW_ADDR_BROADCAST, ESP_NOW_ETH_ALEN);
        else memcpy(temp_dest_addr, EspNowSlave::Instance()->host_mac, ESP_NOW_ETH_ALEN);

        switch(VoicePacketManager::Instance()->voice_packet_status)
        {
            case default_voice_packet_process:
            break;
            case voice_packet_status_start:
            {
                if(get_global_data()->m_is_host)
                {
                    for(int i = 0; i < EspNowHost::Instance()->Bind_slave_mac.count; i++)
                    {
                        //如果发送send成功，只发送一次2s等待15次重发
                        if(VoicePacketManager::Instance()->send_voice_start_message(EspNowHost::Instance()->Bind_slave_mac.bytes[i]))
                        {
                            VoicePacketManager::Instance()->need_send_mac.insert(EspNowHost::Instance()->Bind_slave_mac.bytes[i]);
                        }
                    }
                }
                else
                {   
                    //如果发送send成功，只发送一次2s等待15次重发
                    if(VoicePacketManager::Instance()->send_voice_start_message(EspNowSlave::Instance()->host_mac))
                    {
                        VoicePacketManager::Instance()->need_send_mac.insert(EspNowSlave::Instance()->host_mac);
                    }
                }
                VoicePacketManager::Instance()->voice_packet_status = voice_packet_status_send_packet;
            }
            break;
            case voice_packet_status_send_packet:
            {
                //如果有需要发送的包
                if(VoicePacketManager::Instance()->need_send_mac.count > 0)
                {
                    //说明是第一次发送
                    if(VoicePacketManager::Instance()->received_packets.size() == 0)
                    {
                        for(int i = 0; i < VoicePacketManager::Instance()->total_packets; i++)
                        {
                            VoicePacketManager::Instance()->send_voice_message(i, temp_dest_addr);
                        }
                    }
                    else
                    {
                        //说明是已经发送过包了,则遍历发送received_packets
                        for(auto it = VoicePacketManager::Instance()->received_packets.begin(); it != VoicePacketManager::Instance()->received_packets.end(); it++)
                        {
                            VoicePacketManager::Instance()->send_voice_message(*it, temp_dest_addr);
                        }
                    }
                    VoicePacketManager::Instance()->voice_packet_status = voice_packet_status_stop;
                }
                //如果没有需要发送的包
                else
                {
                    VoicePacketManager::Instance()->voice_packet_status = voice_packet_judge_process;
                }
            }
            break;
            case voice_packet_status_stop:
            {
                if(get_global_data()->m_is_host)
                {
                    for(int i = 0; i < EspNowHost::Instance()->Bind_slave_mac.count; i++)
                    {
                        //保证能发送成功
                        VoicePacketManager::Instance()->send_voice_stop_message(EspNowHost::Instance()->Bind_slave_mac.bytes[i]);
                    }
                }
                else
                {
                    //保证能发送成功
                    VoicePacketManager::Instance()->send_voice_stop_message(EspNowSlave::Instance()->host_mac);
                }
                VoicePacketManager::Instance()->voice_packet_status = voice_packet_status_feedback;
            }
            break;
            case voice_packet_status_feedback:
            break;
            case voice_packet_judge_process:
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if(VoicePacketManager::Instance()->need_send_mac.count == 0)
                {
                    ESP_LOGI(ESP_NOW, "All packets have been received");
                    VoicePacketManager::Instance()->stop_recording_send_task();
                }
            }
            break;
        }
    }
    VoicePacketManager::Instance()->recording_send_task_handle = NULL;
    vTaskDelete(NULL);
}

void VoicePacketManager::start_recording_send_task()
{
    if(recording_send_task_handle == NULL)
    {
        ESP_LOGI(ESP_NOW, "Start recording send task");
        need_stop_recording_send_task = false;
        voice_packet_status = voice_packet_status_start;
        xTaskCreate(recording_send_task, "recording_send_task", 4096, NULL, 10, &recording_send_task_handle);
    }
}

void VoicePacketManager::stop_recording_send_task()
{
    if(recording_send_task_handle != NULL)
    {
        ESP_LOGI(ESP_NOW, "Stop recording send task");
        voice_packet_status = default_voice_packet_process;
        need_stop_recording_send_task = true;
    }
}

