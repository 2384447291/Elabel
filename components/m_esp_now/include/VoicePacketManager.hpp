#ifndef VOICE_PACKET_MANAGER_HPP
#define VOICE_PACKET_MANAGER_HPP

#include <unordered_set>
#include <vector>
#include "esp_now_client.hpp"
#include "MacAdrees.hpp"
enum VoicePacketStatus{
    default_voice_packet_process,
    voice_packet_status_start,
    voice_packet_status_send_packet,
    voice_packet_status_stop,
    voice_packet_status_feedback,
    voice_packet_judge_process,
};


class VoicePacketManager {
public:
    static VoicePacketManager* Instance(){
        static VoicePacketManager instance;
        return &instance;
    }
    //对于发送方是需要发送的包，对越接收方是接收到的包
    std::unordered_set<uint16_t> received_packets;
    uint16_t total_packets = 0;
    uint32_t voice_size = 0;
    MacAddress need_send_mac;

    TaskHandle_t recording_send_task_handle = NULL;
    bool need_stop_recording_send_task = false;
    VoicePacketStatus voice_packet_status = default_voice_packet_process;
    
    // 接收包
    void receive_packet(uint16_t packet_id) {
        if(packet_id >= 1 && packet_id <= total_packets) {
            received_packets.insert(packet_id);
        }
    }

    // 快速检查某个包是否收到
    bool has_packet(uint16_t packet_id) {
        return received_packets.find(packet_id) != received_packets.end();
    }

    // 获取丢失的包ID列表
    std::vector<uint16_t> get_missing_packets() {
        std::vector<uint16_t> missing;
        for(uint16_t i = 1; i <= total_packets; i++) {
            if(!has_packet(i)) {
                missing.push_back(i);
            }
        }
        return missing;
    }

    // 开始录音发送任务
    void start_recording_send_task();
    // 结束录音发送任务
    void stop_recording_send_task();

    bool send_voice_start_message(const espnow_addr_t dest_addr)
    {
        // 获取录音数据大小
        uint32_t recorded_size = MCodec::Instance()->recorded_size;
        voice_size = recorded_size;
        
        // 计算一共要发多少包，向上取整
        uint16_t packet_num = (recorded_size + (MAX_EFFECTIVE_DATA_LEN - 2) - 1) / (MAX_EFFECTIVE_DATA_LEN - 2);
        total_packets = packet_num;

        ESP_LOGI(ESP_NOW, "Recording data size: %d, packet num: %d", recorded_size, packet_num);
        // 首先发送序号为0的包，告知接收方录音数据总大小和包的总数量
        uint8_t info_packet[6];
        // 将录音数据总大小和包的总数量编码到信息包中
        // 使用4字节存储录音数据总大小，2字节存储包的总数量
        info_packet[0] = (recorded_size >> 24) & 0xFF;
        info_packet[1] = (recorded_size >> 16) & 0xFF;
        info_packet[2] = (recorded_size >> 8) & 0xFF;
        info_packet[3] = recorded_size & 0xFF;
        info_packet[4] = (packet_num >> 8) & 0xFF;
        info_packet[5] = packet_num & 0xFF;
        esp_err_t ret;
        ret = EspNowClient::Instance()->send_message(info_packet, 6, Voice_Start_Send, dest_addr);
        if(ret!=ESP_OK){
            return false;
        }
        return true;
    }

    void send_voice_stop_message(const espnow_addr_t dest_addr)
    {
        // 首先发送序号为0的包，告知接收方录音数据总大小和包的总数量
        uint8_t info_packet = 0;
        esp_err_t ret;
        // 发送信息包
        do{
            ret = EspNowClient::Instance()->send_message(&info_packet, 1, Voice_Stop_Send, dest_addr);
        }while(ret!=ESP_OK);

        ESP_LOGI(ESP_NOW, "Sent stop message");
    }

    void send_voice_message(uint16_t voice_packet_id, const espnow_addr_t dest_addr)
    {
        uint32_t current_data_index = voice_packet_id * (MAX_EFFECTIVE_DATA_LEN - 2);
        uint16_t current_packet_size = (voice_packet_id == total_packets) ? 
            (voice_size - current_data_index) : 
            (MAX_EFFECTIVE_DATA_LEN - 2);
        // 准备当前数据包
        uint8_t info_packet[current_packet_size+2];
        // 前两个字节存储包序号
        info_packet[0] = (voice_packet_id >> 8) & 0xFF;  // 序号高字节
        info_packet[1] = voice_packet_id & 0xFF;         // 序号低字节
        // 复制实际数据
        memcpy(&info_packet[2], &MCodec::Instance()->record_buffer[current_data_index], current_packet_size);

        // 发送数据包
        esp_err_t ret;
        do{
            ret = EspNowClient::Instance()->send_message_no_ack(info_packet, current_packet_size+2, Voice_Message, dest_addr);
        }while(ret!=ESP_OK);
    }

    void send_voice_feedback(const espnow_addr_t dest_addr)
    {
        std::vector<uint16_t> missing_packets = get_missing_packets();
        ESP_LOGI(ESP_NOW, "Missing packets: %d", missing_packets.size());

        uint8_t info_packet[MAX_EFFECTIVE_DATA_LEN];
        uint8_t offset = 0;
        info_packet[offset++] = (missing_packets.size()>>8) & 0xFF;
        info_packet[offset++] = missing_packets.size() & 0xFF;
        for(int i = 0; i < missing_packets.size(); i++){
            if(offset+2 > MAX_EFFECTIVE_DATA_LEN){
                ESP_LOGE(ESP_NOW, "Missing packets size is too large");
                break;
            }
            info_packet[offset++] = (missing_packets[i] >> 8) & 0xFF;
            info_packet[offset++] = missing_packets[i] & 0xFF;
        }
        esp_err_t ret;
        do{
            ret = EspNowClient::Instance()->send_message(info_packet, offset, Voice_Feedback, dest_addr);
        }while(ret!=ESP_OK);
    }
};




#endif

