#include "espadf_init.hpp"
#include "codec.hpp"

void espadf_pipline::init()
{   
    ESP_LOGI(TAG, "[ 0 ] Start codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 80);
    audio_hal_enable_pa(espadf_pipline::Instance()->board_handle->audio_hal, false);

    //--------------------------------构建播放的pipline------------------------------------//
    ESP_LOGI(TAG, "[1.1] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_play_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_play_cfg.rb_size = 1024*4;
    pipeline_play = audio_pipeline_init(&pipeline_play_cfg);

    ESP_LOGI(TAG, "[1.2] Create raw_stream_writer");
    raw_stream_cfg_t raw_writer_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_writer_cfg.type = AUDIO_STREAM_WRITER;
    raw_writer_cfg.out_rb_size = 1024*4;
    raw_stream_writer = raw_stream_init(&raw_writer_cfg);

    ESP_LOGI(TAG, "[1.3] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_writer_cfg =  I2S_STREAM_CFG_DEFAULT();
    i2s_writer_cfg.type = AUDIO_STREAM_WRITER;
    //配置i2s_chan_config_t
    i2s_writer_cfg.chan_cfg.dma_desc_num = 4;
    i2s_writer_cfg.chan_cfg.dma_frame_num = 1024;
    i2s_writer_cfg.chan_cfg.auto_clear_after_cb = true;
    i2s_writer_cfg.chan_cfg.auto_clear_before_cb = true;
    i2s_writer_cfg.chan_cfg.allow_pd = true;
    i2s_writer_cfg.chan_cfg.intr_priority = CODEC_PRIORITY;
    //配置i2s_std_cfg_t
    i2s_writer_cfg.std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE);
    i2s_writer_cfg.std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(i2s_data_bit_width_t(I2S_BITS), I2S_SLOT_MODE_MONO);
    i2s_writer_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_stream_writer = i2s_stream_init(&i2s_writer_cfg);

    ESP_LOGI(TAG, "[1.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_play, i2s_stream_writer, "i2s_stream_writer");
    audio_pipeline_register(pipeline_play, raw_stream_writer, "raw_stream_writer");

    ESP_LOGI(TAG, "[1.5] Link it together raw_stream_writer-->i2s_stream_writer-->[codec_chip]");
    const char *link_play_tag[2] = {"raw_stream_writer", "i2s_stream_writer"};
    audio_pipeline_link(pipeline_play, &link_play_tag[0], 2);
    //--------------------------------构建播放的pipline------------------------------------//


    //--------------------------------构建读取的pipline------------------------------------//
    ESP_LOGI(TAG, "[2.1] Create audio pipeline for record");
    audio_pipeline_cfg_t pipeline_rec_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_rec_cfg.rb_size = 1024*4;
    pipeline_record = audio_pipeline_init(&pipeline_rec_cfg);

    ESP_LOGI(TAG, "[2.2] Create raw_stream_reader");
    raw_stream_cfg_t raw_reader_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_reader_cfg.type = AUDIO_STREAM_READER;
    raw_reader_cfg.out_rb_size = 1024*4;
    raw_stream_reader = raw_stream_init(&raw_reader_cfg);

    ESP_LOGI(TAG, "[2.3] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_reader_cfg =  I2S_STREAM_CFG_DEFAULT();
    i2s_reader_cfg.type = AUDIO_STREAM_READER;
    //配置i2s_chan_config_t
    i2s_reader_cfg.chan_cfg.dma_desc_num = 4;
    i2s_reader_cfg.chan_cfg.dma_frame_num = 1024;
    i2s_reader_cfg.chan_cfg.auto_clear_after_cb = true;
    i2s_reader_cfg.chan_cfg.auto_clear_before_cb = true;
    i2s_reader_cfg.chan_cfg.allow_pd = true;
    i2s_reader_cfg.chan_cfg.intr_priority = 7;
    //配置i2s_std_cfg_t
    i2s_reader_cfg.std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE);
    i2s_reader_cfg.std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(i2s_data_bit_width_t(I2S_BITS), I2S_SLOT_MODE_MONO);
    i2s_reader_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_stream_record = i2s_stream_init(&i2s_reader_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_record, i2s_stream_record, "i2s_stream_record");
    audio_pipeline_register(pipeline_record, raw_stream_reader, "raw_stream_reader");

    ESP_LOGI(TAG, "[2.5] Link it together [codec_chip]-->i2s_stream_record-->raw_stream_reader");
    const char *link_record_tag[2] = {"i2s_stream_record", "raw_stream_reader"};
    audio_pipeline_link(pipeline_record, &link_record_tag[0], 2);

    //--------------------------------构建读取的pipline------------------------------------//
    ESP_LOGI(TAG, "[3.0] Create event interface");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(pipeline_play, evt);
    audio_pipeline_set_listener(pipeline_record, evt);
}
