#ifndef CODEC_HPP
#define CODEC_HPP

#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
 #include "esp_system.h"
#include <sys/stat.h>
#include "driver/i2s_std.h"
#include "driver/i2c.h"

#define I2C_PORT I2C_NUM_0
#define I2S_PORT I2S_NUM_0 
#define I2S_BITS_PER_SAMPLE 16
#define MIC_SAMPLE_RATE 16000
#define SPEAKER_SAMPLE_RATE 16000
#define I2S_CHANNEL_NUM 1

#define TAG "M_CODEC"
#define READ_BLOCK_SIZE 1024      
#define BytesPerSecond (MIC_SAMPLE_RATE * I2S_CHANNEL_NUM * I2S_BITS_PER_SAMPLE / 8)
#define RecordTime 8
#define ShutdownTime 0.2f
#define FILE_PATH "/fat/mic.raw"

typedef enum {
    default_speaker,
    mic,
    music,
} Speakertype;

class MCodec {
public:
    MCodec(){
        codec_dev = NULL;
        mic_task = NULL;
        speaker_task = NULL;
    };
    void init();
    void deinit();
    
    void start_record();
    void stop_record();
    
    void play_record(const uint8_t* data, size_t size);
    void play_music(const char* filename);
    void play_mic();
    void stop_play();
    
    void set_volume(uint8_t volume);
    void set_mic_gain(float gain);

    static MCodec* Instance() {
        static MCodec instance;
        return &instance;
    }
    
    esp_codec_dev_handle_t codec_dev = NULL;
    TaskHandle_t speaker_task = NULL;
    TaskHandle_t mic_task = NULL;

    Speakertype speaker_type = default_speaker;

    // FAT文件系统
    wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
    esp_vfs_fat_mount_config_t mount_config;  //依托vfs管理fat文件系统

    // 播放数据
    const uint8_t* play_data = NULL;
    size_t play_data_size = 0;
    // 播放文件
    FILE *play_file = NULL;
    // 设置默认配置
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .channel = (uint8_t)I2S_CHANNEL_NUM,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = SPEAKER_SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    uint8_t codec_gain = 25;
    uint8_t codec_vol = 90;
    
    void open_dev(uint32_t sample_rate)
    {
        fs.sample_rate = sample_rate;
        esp_codec_dev_open(codec_dev, &fs);
        esp_codec_dev_set_out_vol(codec_dev, codec_vol);
        esp_codec_dev_set_in_gain(codec_dev, codec_gain);
    }
    void close_dev()
    {
        esp_codec_dev_close(codec_dev);
    }
};

#endif