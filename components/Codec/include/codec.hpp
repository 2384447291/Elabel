#ifndef CODEC_HPP
#define CODEC_HPP

#include "freertos/ringbuf.h"
#include "esp_codec_dev.h"

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
    
    void play_record();
    void stop_play();
    
    void set_volume(uint8_t volume);
    void set_mic_gain(float gain);

    static MCodec* Instance() {
        static MCodec instance;
        return &instance;
    }
    
    RingbufHandle_t mic_buf = NULL;
    RingbufHandle_t speaker_buf = NULL;
    esp_codec_dev_handle_t codec_dev = NULL;
    TaskHandle_t speaker_task = NULL;
    TaskHandle_t mic_task = NULL;
};

#endif