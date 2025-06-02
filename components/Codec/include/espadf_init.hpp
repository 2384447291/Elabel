#ifndef ESPADF_INIT_HPP
#define ESPADF_INIT_HPP

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "es8311.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "algorithm_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "embed_flash_stream.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "audio_idf_version.h"
#include "board.h"
#include "music.hpp"
#include "raw_stream.h"

class espadf_pipline
{
public:
    espadf_pipline(){};
    void init();
    void reset(audio_pipeline_handle_t& _pipeline)
    {
        audio_pipeline_stop(_pipeline);
        audio_pipeline_wait_for_stop(_pipeline);
        audio_pipeline_reset_ringbuffer(_pipeline);
        audio_pipeline_reset_elements(_pipeline);
    };

    audio_event_iface_handle_t evt; 
    audio_pipeline_handle_t pipeline_play;
    audio_pipeline_handle_t pipeline_record;

    audio_element_handle_t raw_stream_writer;
    audio_element_handle_t i2s_stream_writer;

    audio_element_handle_t i2s_stream_record;
    audio_element_handle_t fatfs_stream_writer;
    audio_element_handle_t raw_stream_reader;

    audio_board_handle_t board_handle;

    static espadf_pipline* Instance() {
        static espadf_pipline instance;
        return &instance;
    }
};

#endif