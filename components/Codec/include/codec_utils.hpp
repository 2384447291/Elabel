#ifndef _CODEC_UTILS_HPP_
#define _CODEC_UTILS_HPP_

#include "driver/i2s.h"
#include "driver/i2c.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"
#include "codec.hpp"
#include "codec_utils.hpp"
#include "esp_log.h"
#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_data_if.h"
#include "audio_codec_gpio_if.h"


#define I2C_SCL_PIN     GPIO_NUM_3
#define I2C_SDA_PIN     GPIO_NUM_20
#define I2S_MCLK_PIN    GPIO_NUM_9
#define I2S_BCK_PIN     GPIO_NUM_19
#define I2S_WS_PIN      GPIO_NUM_18
#define I2S_DO_PIN      GPIO_NUM_17
#define I2S_DI_PIN      GPIO_NUM_8
#define AMP_EN_PIN      GPIO_NUM_38


void i2c_init(i2c_port_t port)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 100000                 // I2C时钟频率100kHz
        },
    };
    ESP_ERROR_CHECK(i2c_param_config(port, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(port, i2c_cfg.mode, 0, 0, 0));
}

void i2s_init(i2s_port_t port)
{
    //默认的通道数目就是1
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t) (I2S_MODE_TX | I2S_MODE_RX | I2S_MODE_MASTER),
        .sample_rate = SPEAKER_SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)I2S_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
        .dma_buf_count = 2,
        .dma_buf_len = 512,
        .use_apll = true,
        .tx_desc_auto_clear = true,
    };
    i2s_driver_install(port, &i2s_config, 0, NULL);
    i2s_pin_config_t i2s_pin_cfg = {
        .mck_io_num = I2S_MCLK_PIN,
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_DO_PIN,
        .data_in_num = I2S_DI_PIN,
    };
    i2s_set_pin(port, &i2s_pin_cfg);
}

void i2s_deinit(i2s_port_t port)
{
    i2s_driver_uninstall(port);
} 


void i2c_deinit(i2c_port_t port)
{
    i2c_driver_delete(port);
}

void esp_codec_deinit(esp_codec_dev_handle_t &codec_dev)
{
    if(codec_dev!=NULL)
    {
        esp_codec_dev_close(codec_dev);
        esp_codec_dev_delete(codec_dev);
        codec_dev = NULL;
    } 
    i2c_deinit(I2C_PORT);
    i2s_deinit(I2S_PORT);
}

void esp_codec_init(esp_codec_dev_handle_t &codec_dev)
{
    //初始化I2S
    i2s_init(I2S_PORT);
    //初始化data_if(数据口)
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_PORT,
        .rx_handle = NULL,
        .tx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if);

    //初始化I2C
    i2c_init(I2C_PORT);
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_PORT,
        .addr = ES8311_CODEC_DEFAULT_ADDR
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    //初始化es8311cfg
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = AMP_EN_PIN,
        .use_mclk = true,
    };
    //初始化condec_if
    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

    //初始化esp_codec_dev的config参数
    esp_codec_dev_cfg_t codec_cfg ={
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,//芯片控制口
        .data_if = data_if,//芯片数据口
    };
    codec_dev = esp_codec_dev_new(&codec_cfg);
    assert(codec_dev);
}

#endif