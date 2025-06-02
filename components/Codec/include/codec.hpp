#ifndef CODEC_HPP
#define CODEC_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
 #include "esp_system.h"
#include <sys/stat.h>
#include "global_message.h"


#define I2S_SAMPLE_RATE           24000
#define I2S_BITS                  24
#define READ_BLOCK_SIZE           1024
#define CODEC_PRIORITY            5

#define TAG "M_CODEC"
#define BytesPerSecond (I2S_SAMPLE_RATE * I2S_BITS / 8)
#define RecordTime 8
#define ShutdownTime 0.4f
#define BeforeRecordTime 0.1f
#define FILE_PATH "/fat/mic.raw"

void play_button_sound();
void play_start_task_sound();
void play_finish_task_sound();

typedef enum {
    default_speaker,
    mic,
    music,
} Speakertype;

class MCodec {
public:
    MCodec(){
        mic_task = NULL;
        speaker_task = NULL;
    };
    void init();
    void deinit();
    
    void start_record();
    void stop_record();
    
    void play_record(const uint8_t* data = NULL, size_t size = 0);
    void play_music(const char* filename);
    void play_mic();
    void stop_play();

    static MCodec* Instance() {
        static MCodec instance;
        return &instance;
    }
    
    TaskHandle_t speaker_task = NULL;
    TaskHandle_t mic_task = NULL;

    Speakertype speaker_type = default_speaker;

    // FAT文件系统用来存储录音音效
    wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

    // 播放数据
    const uint8_t* play_data = NULL;
    size_t play_data_size = 0;
    // 播放文件
    FILE *play_file = NULL;

    //增益
    uint8_t codec_gain = 25;
    //播放音量
    uint8_t *codec_vol = &(get_global_data()->m_device_info.sound_volume);
};

#endif