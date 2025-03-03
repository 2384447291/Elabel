#ifndef CODEC_HPP
#define CODEC_HPP

#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

#define RECORD_BUFFER_SIZE (120 * 1024)  // 120KB 的录音缓冲区

typedef enum {
    mic,
    spiffs,
} Speakertype;

class MCodec {
public:
    MCodec(){
        codec_dev = NULL;
        mic_task = NULL;
        speaker_task = NULL;
        record_buffer = NULL;
        recorded_size = 0;
    };
    void init();
    void deinit();
    
    void start_record();
    void stop_record();
    
    void play_record(const uint8_t* data, size_t size);
    void play_music(const char* filename);
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

    Speakertype speaker_type = spiffs;

    // 录音数据
    uint8_t* record_buffer = NULL;  // PSRAM 中的录音缓冲区
    size_t recorded_size = 0;       // 已录制的数据大小

    // 播放数据
    const uint8_t* play_data = NULL;
    size_t play_data_size = 0;
    // 播放文件
    FILE *play_file = NULL;
};

#endif