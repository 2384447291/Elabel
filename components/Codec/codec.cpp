#include "codec_utils.hpp"
#include "codec.hpp"

#define TAG "M_CODEC"
#define BUFFER_SIZE (80 * 1024)  // 80KB buffer size
#define READ_BLOCK_SIZE 1024     // 每次读取的数据块大小
// 采样率 (I2S_SAMPLE_RATE) = 16000 Hz
// 采样位数 (I2S_BITS_PER_SAMPLE) = 16 bits = 2 bytes
// 通道数 (I2S_CHANNEL_NUM) = 1 (单声道)
// 计算每秒数据量：
// 每秒采样数 = 16000
// 每个采样占用字节数 = 2 bytes
// 通道数 = 1
// 每秒总字节数 = 16000*2*1 = 32000 bytes/s

// 用于任务间通信的标志
static volatile bool should_stop_recording = false;
static volatile bool should_stop_playing = false;

// 录音任务函数
static void mic_task_func(void* arg) {
    ESP_LOGI(TAG, "Mic task started");
    uint8_t* buffer = (uint8_t*)malloc(READ_BLOCK_SIZE);
    if(buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for mic task");
        vTaskDelete(NULL);
        return;
    }
    size_t total_bytes = 0;
    should_stop_recording = false;
    
    while(!should_stop_recording) {
        // 使用esp_codec_dev_read直接从codec读取数据
        esp_err_t ret = esp_codec_dev_read(MCodec::Instance()->codec_dev, buffer, READ_BLOCK_SIZE);
        if(ret == ESP_OK) {
            // 检查是否达到缓冲区限制
            if(total_bytes + READ_BLOCK_SIZE >= BUFFER_SIZE) {
                ESP_LOGI(TAG, "Buffer full (%d bytes), stopping recording", total_bytes);
                should_stop_recording = true;
                break;
            }
            
            if(xRingbufferSend(MCodec::Instance()->mic_buf, buffer, READ_BLOCK_SIZE, pdMS_TO_TICKS(100)) == pdTRUE) {
                total_bytes += READ_BLOCK_SIZE;
                ESP_LOGI(TAG, "Recording: %d bytes (%.1f seconds)", total_bytes, (float)total_bytes/32000.0f);
            } else {
                ESP_LOGE(TAG, "Failed to write to mic buffer, total_bytes=%d", total_bytes);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read from codec, err=%d", ret);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    free(buffer);
    ESP_LOGI(TAG, "Mic task ended, total recorded: %d bytes", total_bytes);

    // 复制数据到speaker_buf
    size_t item_size;
    uint8_t* item;
    size_t total_copied = 0;
    
    ESP_LOGI(TAG, "Copying data from mic_buf to speaker_buf");
    while((item = (uint8_t*)xRingbufferReceive(MCodec::Instance()->mic_buf, &item_size, 0)) != NULL) {
        if(xRingbufferSend(MCodec::Instance()->speaker_buf, item, item_size, pdMS_TO_TICKS(100)) == pdTRUE) {
            total_copied += item_size;
            ESP_LOGI(TAG, "Copied %d bytes, total: %d bytes", item_size, total_copied);
        } else {
            ESP_LOGE(TAG, "Failed to copy %d bytes to speaker buffer", item_size);
        }
        vRingbufferReturnItem(MCodec::Instance()->mic_buf, item);
    }
    ESP_LOGI(TAG, "Finished copying, total: %d bytes (%.1f seconds)", total_copied, (float)total_copied/32000.0f);

    // 清除任务句柄
    MCodec::Instance()->mic_task = NULL;
    vTaskDelete(NULL);
}

// 播放任务函数
static void speaker_task_func(void* arg) {
    ESP_LOGI(TAG, "Speaker task started");
    size_t item_size;
    uint8_t* item;
    size_t total_played = 0;
    should_stop_playing = false;
    
    while(!should_stop_playing) {
        item = (uint8_t*)xRingbufferReceive(MCodec::Instance()->speaker_buf, &item_size, pdMS_TO_TICKS(100));
        if(item != NULL) {
            ESP_LOGI(TAG, "Playing chunk: %d bytes (%.1f seconds)", item_size, (float)item_size/32000.0f);
            // 使用esp_codec_dev_write直接写入数据到codec
            esp_err_t ret = esp_codec_dev_write(MCodec::Instance()->codec_dev, item, item_size);
            if(ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write to codec, err=%d", ret);
            } else {
                total_played += item_size;
                ESP_LOGI(TAG, "Total played: %d bytes (%.1f seconds)", total_played, (float)total_played/32000.0f);
            }
            vRingbufferReturnItem(MCodec::Instance()->speaker_buf, item);
        } else {
            // 如果没有数据可读，说明播放完成
            ESP_LOGI(TAG, "No more data to play, stopping playback");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "Speaker task ended, total played: %d bytes", total_played);
    // 清除任务句柄
    MCodec::Instance()->speaker_task = NULL;
    vTaskDelete(NULL);
}

void MCodec::init()
{
    // 先调用esp_codec_init()创建codec设备
    esp_codec_init(codec_dev);
    // 确保codec_dev已经创建成功
    assert(codec_dev);
    
    // 设置默认配置
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .channel = (uint8_t)I2S_CHANNEL_NUM,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = I2S_SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    
    esp_codec_dev_open(codec_dev, &fs);
    esp_codec_dev_set_out_vol(codec_dev, 100.0);
    esp_codec_dev_set_in_gain(codec_dev, 30.0);
    ESP_LOGI(TAG, "Codec initialized");

    // 创建环形缓冲区
    mic_buf = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if(mic_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create mic ring buffer, size: %d bytes", BUFFER_SIZE);
    } else {
        ESP_LOGI(TAG, "Created mic ring buffer successfully");
    }

    speaker_buf = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if(speaker_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create speaker ring buffer, size: %d bytes", BUFFER_SIZE);
    } else {
        ESP_LOGI(TAG, "Created speaker ring buffer successfully");
    }

    if(mic_buf != NULL && speaker_buf != NULL) {
        ESP_LOGI(TAG, "Both buffers created successfully with size %d bytes (%.1f seconds of audio)", 
                 BUFFER_SIZE, (float)BUFFER_SIZE/32000.0f);
    }
}

void MCodec::deinit()
{
    stop_record();
    stop_play();
    if(mic_buf) {
        vRingbufferDelete(mic_buf);
        mic_buf = NULL;
    }
    if(speaker_buf) {
        vRingbufferDelete(speaker_buf);
        speaker_buf = NULL;
    }
    esp_codec_dev_close(codec_dev);
    esp_codec_deinit(codec_dev);
}

void MCodec::start_record()
{
    if(mic_task == NULL) {
        ESP_LOGI(TAG, "Creating mic task");
        xTaskCreate(mic_task_func, "mic_task", 4096, this, 5, &mic_task);
    }
    else{
        ESP_LOGI(TAG, "Mic task already exists");
    }
}

void MCodec::stop_record()
{
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

void MCodec::play_record()
{
    if(speaker_task == NULL && speaker_buf != NULL) {
        ESP_LOGI(TAG, "Creating speaker task");
        xTaskCreate(speaker_task_func, "speaker_task", 4096, this, 5, &speaker_task);
    }
    else{
        ESP_LOGI(TAG, "Speaker task already exists");
    }
}

void MCodec::stop_play()
{
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

void MCodec::set_volume(uint8_t volume)
{
    if(codec_dev) {
        esp_codec_dev_set_out_vol(codec_dev, (float)volume);
        ESP_LOGI(TAG, "Volume set to %d", volume);
    }
}

void MCodec::set_mic_gain(float gain)
{
    if(codec_dev) {
        esp_codec_dev_set_in_gain(codec_dev, gain);
        ESP_LOGI(TAG, "Mic gain set to %.1f", gain);
    }
}

