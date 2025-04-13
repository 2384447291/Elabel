#include "codec_utils.hpp"
#include "codec.hpp"
#include "esp_heap_caps.h" 
#include <string.h>  
#include "music.hpp"

#define TAG "M_CODEC"
#define READ_BLOCK_SIZE 1024      // 减小读取块大小以减少瞬时内存占用
#define BytesPerSecond (MIC_SAMPLE_RATE * I2S_CHANNEL_NUM * I2S_BITS_PER_SAMPLE / 8)

// 用于任务间通信的标志
static volatile bool should_stop_recording = false;
static volatile bool should_stop_playing = false;

// 录音任务函数
static void mic_task_func(void* arg) {
    MCodec::Instance()->play_music("ding");
    while(MCodec::Instance()->speaker_task!=NULL)
    {
        ESP_LOGI(TAG,"Recording Guidance playing");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    MCodec::Instance()->open_dev(MIC_SAMPLE_RATE);
    ESP_LOGI(TAG, "Mic task started");
    MCodec* codec = MCodec::Instance();
    uint8_t buffer[READ_BLOCK_SIZE];

    // 重置录音大小
    codec->recorded_size = 0;
    codec->target_record_size = 0;
    should_stop_recording = false;
    
    while(!should_stop_recording) {
        // 使用esp_codec_dev_read直接从codec读取数据
        esp_err_t ret = esp_codec_dev_read(codec->codec_dev, buffer, READ_BLOCK_SIZE);
        if(ret == ESP_OK) {
            // 检查是否达到缓冲区限制
            if(codec->recorded_size + READ_BLOCK_SIZE >= RECORD_BUFFER_SIZE) {
                ESP_LOGI(TAG, "Buffer full (%d bytes), stopping recording", codec->recorded_size);
                should_stop_recording = true;
                break;
            }
            
            // 复制数据到录音缓冲区
            memcpy(codec->record_buffer + codec->recorded_size, buffer, READ_BLOCK_SIZE);
            codec->recorded_size += READ_BLOCK_SIZE;
            // ESP_LOGI(TAG, "Recording: %d bytes (%.1f seconds)", 
            //         codec->recorded_size, (float)codec->recorded_size/BytesPerSecond);
        } else {
            ESP_LOGE(TAG, "Failed to read from codec, err=%d", ret);
        }
    }
    codec->target_record_size = codec->recorded_size;
    ESP_LOGI(TAG, "Mic task ended, total recorded: %d bytes", codec->recorded_size);
    //关闭设备
    codec->close_dev();
    // 清除任务句柄
    codec->mic_task = NULL;
    vTaskDelete(NULL);
}

// 播放任务函数
static void speaker_task_func(void* arg) {
    ESP_LOGI(TAG, "Speaker task started");
    MCodec* codec = MCodec::Instance();
    should_stop_playing = false;

    uint8_t buffer[READ_BLOCK_SIZE];
    size_t total_played = 0;

    while(!should_stop_playing) 
    {
        size_t bytes_to_play = 0;
        
        // 根据播放模式获取要播放的数据
        if(codec->speaker_type == spiffs) {
            // 从文件读取数据
            bytes_to_play = fread(buffer, 1, READ_BLOCK_SIZE, codec->play_file);
            if(bytes_to_play == 0) break;  // 文件读取完毕
        } else if(codec->speaker_type == mic || codec->speaker_type == music) {
            // 从内存数组读取数据
            size_t remaining = codec->play_data_size - total_played;
            if(remaining == 0) break;  // 数据播放完毕
            
            bytes_to_play = (remaining > READ_BLOCK_SIZE) ? READ_BLOCK_SIZE : remaining;
            memcpy(buffer, codec->play_data + total_played, bytes_to_play);
        }

        // 播放数据
        esp_err_t ret = esp_codec_dev_write(codec->codec_dev, buffer, bytes_to_play);
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "播放失败: %d", ret);
            break;
        }

        total_played += bytes_to_play;
        // ESP_LOGI(TAG, "Playing: %d bytes (%.1f seconds)", 
        //         total_played, (float)total_played/BytesPerSecond);
    }
    
    // 清理工作
    if(codec->speaker_type == spiffs && codec->play_file != NULL) {
        fclose(codec->play_file);
        codec->play_file = NULL;
    }
    //关闭设备
    codec->close_dev();
    ESP_LOGI(TAG, "播放结束，总共播放: %d bytes", total_played);
    codec->speaker_task = NULL;
    vTaskDelete(NULL);
}

void MCodec::init()
{
    // 先调用esp_codec_init()创建codec设备
    esp_codec_init(codec_dev);
    // 确保codec_dev已经创建成功
    assert(codec_dev);
    //关闭时反使能
    esp_codec_set_disable_when_closed(codec_dev,true);
    ESP_LOGI(TAG, "Codec initialized");

    // 在 PSRAM 中分配录音缓冲区
    record_buffer = (uint8_t*)heap_caps_malloc(RECORD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (record_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate record buffer in PSRAM");
        return;
    }
    ESP_LOGI(TAG, "Allocated record buffer in PSRAM: %d bytes (%.1f seconds)", 
             RECORD_BUFFER_SIZE, (float)RECORD_BUFFER_SIZE/BytesPerSecond);

    // 挂载 SPIFFS 文件系统
    // esp_vfs_spiffs_conf_t conf = {
    //     .base_path = "/spiffs",       
    //     .partition_label = "music",  
    //     .max_files = 5,  
    //     .format_if_mount_failed = true
    // };

    // esp_err_t ret = esp_vfs_spiffs_register(&conf);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "SPIFFS 文件系统挂载失败");
    //     return;
    // }
}

void MCodec::deinit()
{
    stop_record();
    stop_play();
    
    if(record_buffer) {
        heap_caps_free(record_buffer);
        record_buffer = NULL;
    }

    // 卸载 SPIFFS 文件系统
    // esp_vfs_spiffs_unregister("music");
    
    esp_codec_dev_close(codec_dev);
    esp_codec_deinit(codec_dev);
}

void MCodec::set_volume(uint8_t volume)
{
    if(codec_dev) {
        MCodec::Instance()->codec_vol = volume;
        ESP_LOGI(TAG, "Volume set to %d", volume);
    }
}

void MCodec::set_mic_gain(float gain)
{
    if(codec_dev) {
        MCodec::Instance()->codec_gain = gain;
        ESP_LOGI(TAG, "Mic gain set to %.1f", gain);
    }
}


void MCodec::start_record()
{
    if(mic_task!=NULL) 
    {
        ESP_LOGW(TAG, "mic_task already exists, No need to start recording");
        return;
    }
    if(record_buffer == NULL) {
        ESP_LOGE(TAG, "Record buffer not initialized");
        return;
    }

    if(mic_task != NULL) {
        ESP_LOGW(TAG, "Mic task already exists");
        return;
    }
    recorded_size = 0;
    target_record_size = 0;
    ESP_LOGI(TAG, "Starting recording...");
    xTaskCreate(mic_task_func, "mic_task", 4096, NULL, 0, &mic_task);
}

void MCodec::stop_record()
{
    if(mic_task==NULL) 
    {
        ESP_LOGW(TAG, "mic_task is NULL, No need to stop recording");
        return;
    }
    ESP_LOGI(TAG, "Stopping recording");
    if(mic_task) {
        // 设置停止标志，让任务自己结束
        should_stop_recording = true;
        // 等待任务结束
        while(mic_task != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Recording stopped");
    } else {
        ESP_LOGW(TAG, "stop_record called but mic_task is NULL");
    }
}

// void MCodec::play_music(const char* filename)
// {
//     if(speaker_task!=NULL)
//     {
//         ESP_LOGW(TAG, "Speaker task already exists, stopping previous playback");
//         return;
//     }
//     speaker_type = spiffs;
//     // 打开文件 spiffs+filename.pcm
//     char path[64];
//     snprintf(path, sizeof(path), "/spiffs/%s.pcm", filename);
//     play_file = fopen(path, "rb");
//     if (play_file == NULL) {
//         ESP_LOGE(TAG, "无法打开文件");
//         return;
//     }
//     fseek(play_file, 0, SEEK_END);
//     size_t size = ftell(play_file);
//     fseek(play_file, 0, SEEK_SET);
//     ESP_LOGI(TAG, "Creating speaker task for %d bytes (%.1f seconds)", 
//              size, (float)size/BytesPerSecond);
//     xTaskCreate(speaker_task_func, "speaker_task", 4096, NULL, 5, &speaker_task);
// }
void MCodec::play_mic()
{
    speaker_type = mic;
    play_record(MCodec::Instance()->record_buffer, MCodec::Instance()->recorded_size);
}

void MCodec::play_music(const char* filename)
{
    // 根据文件名获取对应的音频数组
    const uint8_t* audio_data = nullptr;
    size_t audio_size = 0;

    if(strcmp(filename, "bell") == 0) {
        audio_data = bell;
        audio_size = bell_size;
    } else if(strcmp(filename, "ding") == 0) {
        audio_data = ding; 
        audio_size = ding_size;
    } else if(strcmp(filename, "open") == 0) {
        audio_data = open;
        audio_size = open_size;
    } else if(strcmp(filename, "stop") == 0) {
        audio_data = stop;
        audio_size = stop_size;
    } else {
        ESP_LOGE(TAG, "未找到对应的音频文件: %s", filename);
        return;
    }
    speaker_type = music;
    play_record(audio_data, audio_size);
}

void MCodec::play_record(const uint8_t* data, size_t size)
{
    if(speaker_task!=NULL) 
    {
        ESP_LOGW(TAG, "Speaker task already exists, stopping previous playback");
        return;
    }
    if (data == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid play parameters: data=%p, size=%d", data, size);
        return;
    }

    if(speaker_task != NULL) {
        ESP_LOGW(TAG, "Speaker task already exists, stopping previous playback");
        return;
    }
    if(speaker_type == music)
    {
        open_dev(SPEAKER_SAMPLE_RATE);
    }
    else
    {
        open_dev(MIC_SAMPLE_RATE);
    }
    // 保存播放数据的指针和大小
    play_data = data;
    play_data_size = size;

    ESP_LOGI(TAG, "Creating speaker task for %d bytes (%.1f seconds)", 
             size, (float)size/BytesPerSecond);
    xTaskCreate(speaker_task_func, "speaker_task", 4096, NULL, 0, &speaker_task);
}

void MCodec::stop_play()
{
    if(speaker_task==NULL) 
    {
        ESP_LOGW(TAG, "speaker_task is NULL, No need to stop playback");
        return;
    }
    ESP_LOGI(TAG, "Stopping playback");
    if(speaker_task) {
        // 设置停止标志，让任务自己结束
        should_stop_playing = true;
        // 等待任务结束
        while(speaker_task != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Playback stopped");
    } else {
        ESP_LOGW(TAG, "stop_play called but speaker_task is NULL");
    }
}




