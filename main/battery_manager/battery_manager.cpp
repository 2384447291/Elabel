#include "battery_manager.hpp"
#include "global_time.h"
#include "esp_log.h"
#include "control_driver.hpp"
#include "driver/adc.h"
#include "driver/adc_common.h"
#include "esp_adc_cal.h"
#include "global_draw.h"
#include "freertos/timers.h"
#include "global_message.h"
#define TAG "BATTERY_MANAGER"

#undef ESP_LOGI
#define ESP_LOGI(tag, format, ...) 


static esp_adc_cal_characteristics_t adc_chars;

static void shutdown_timer_callback(TimerHandle_t xTimer) {
    // 关闭电源
    BatteryManager::Instance()->setPowerState(false);
    // 删除定时器
    xTimerDelete(xTimer, 0);    
}

void power_off_system()
{
    //锁定屏幕
    lock_lvgl();
    switch_screen(ui_ShutdownScreen);
    //释放lvgl
    release_lvgl();
    // 播放关机音乐
    // ControlDriver::Instance()->buzzer.playDownMusic();

    // 创建2秒倒计时定时器
    TimerHandle_t shutdownTimer = xTimerCreate(
        "ShutdownTimer",
        pdMS_TO_TICKS(2000),
        pdFALSE,  // 单次触发
        nullptr,
        shutdown_timer_callback
    );
    
    if (shutdownTimer != nullptr) {
        xTimerStart(shutdownTimer, 0);
    }
}

void BatteryManager::battery_update_task(void* parameters) {
    while (true) {
        //1s检查一次电量
        vTaskDelay(1000 / portTICK_PERIOD_MS);  
        // 检测充电状态变化
        
        static bool lastChargingStatus = false;
        if (!lastChargingStatus && BatteryManager::Instance()->isUsbConnected()) {
            // 从未充电变为充电状态,播放充电音乐
            // ControlDriver::Instance()->buzzer.playChargingMusic();
        }
        lastChargingStatus = BatteryManager::Instance()->isUsbConnected();

        if(BatteryManager::Instance()->getBatteryLevel() <= LOWLEST_VOLTAGE) {
            lock_lvgl();
            if(get_global_data()->m_language == English)
            {
                set_text_with_change_font(ui_ShutdownGuide, "No Battery Please Charge", true);
            }
            else if(get_global_data()->m_language == Chinese)
            {
                set_text_with_change_font(ui_ShutdownGuide, "电量不足请充电", false);
            }
            release_lvgl();
            power_off_system();
            ESP_LOGI(TAG, "Battery level is too low, power off system");
        }
    }
}


void BatteryManager::init() {
    // 配置电池开关控制引脚（输出）
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BATTERY_POWER_CTRL);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // 配置USB连接检测引脚（输入）
    io_conf.pin_bit_mask = (1ULL << USB_CONNECT_DET);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // 初始化ADC1
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC1_CHAN, ADC_ATTEN);
    
    // 特性曲线校准
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_chars);

    // 初始化完成后默认开启电源
    setPowerState(true);
    
    // 创建电池电量更新线程
    xTaskCreate(battery_update_task, "battery_update", 3072, this, 10, nullptr);
}


void BatteryManager::setPowerState(bool enable) {
    gpio_set_level(BATTERY_POWER_CTRL, enable ? 1 : 0);
    powerEnabled = enable;
    ESP_LOGI(TAG, "Battery power state: %s", enable ? "ON" : "OFF");
}

float BatteryManager::getBatteryLevel() {
    uint32_t adc_reading = 0;
    
    // 多次采样取平均值
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_reading += adc1_get_raw(ADC1_CHAN);
    }
    adc_reading /= ADC_SAMPLES;
    
    // 将ADC读数转换为实际电压值（mV）
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    
    // 由于使用分压电路，这里需要根据实际分压比例计算真实电池电压
    // 假设使用100K和100K的分压电阻，则实际电压为ADC读数的2倍
    float actual_voltage = voltage * 2.0f / 1000.0f;  // 转换为V
    
    //4.18-->4.21有点小误差
    ESP_LOGI(TAG, "Battery voltage: %.3fV", actual_voltage);
    ESP_LOGI(TAG, "Is charging: %s", chargingStatus ? "Yes" : "No");
    ESP_LOGI(TAG, "Is Full Power: %s", fullPowerStatus ? "Yes" : "No");
    ESP_LOGI(TAG, "Is Usb Connected: %s", isUsbConnected() ? "Yes" : "No");
    batteryLevel = actual_voltage;
    return actual_voltage;
}
