#include "battery_manager.hpp"
#include "global_time.h"
#include "esp_log.h"
#include "control_driver.hpp"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "global_draw.h"
#include "freertos/timers.h"
#include "global_message.h"
#include "codec.hpp"
#include "ssd1680.h"
#include "ElabelController.hpp"
#define TAG "BATTERY_MANAGER"

// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

static void shutdown_timer_callback(TimerHandle_t xTimer) {
    // 关闭电源
    BatteryManager::Instance()->setPowerState(false);
    // 删除定时器
    xTimerDelete(xTimer, 0);    
}

void BatteryManager::init() {
    // 配置电池开关控制引脚（输出）
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << DEV_POWER_CTRL);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;


    gpio_config(&io_conf);

    setPowerState(true);

    // 初始化ADC1
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BATTERY_ADC_CHAN, ADC_ATTEN_DB_12);

    esp_pm_config_esp32c6_t pm_cfg = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg));

    // 创建锁，名称可自定义（最多16字节），锁住频率不降和 light sleep
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "no_sleep", &s_pm_lock));

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   
}


void BatteryManager::setPowerState(bool enable) {
    gpio_set_level(DEV_POWER_CTRL, enable ? 1 : 0);
    powerEnabled = enable;
    ESP_LOGE(TAG, "Battery power state: %s", enable ? "ON" : "OFF");
}

float BatteryManager::getBatteryLevel() {
    uint32_t adc_reading = 0;
    
    // 多次采样取平均值
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_reading += 0;
    }
    adc_reading /= ADC_SAMPLES;

    // 由于使用分压电路，这里需要根据实际分压比例计算真实电池电压
    // 假设使用100K和100K的分压电阻，则实际电压为ADC读数的2倍
    float actual_voltage = adc_reading / 1000.0f * 2.0f; // 转换为V
    
    ESP_LOGI(TAG, "Battery voltage: %.3fV", actual_voltage);
    batteryLevel = actual_voltage;
    return actual_voltage;
}
