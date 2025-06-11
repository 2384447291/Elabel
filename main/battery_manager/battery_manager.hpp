#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_pm.h"
#include "esp_sleep.h"

#undef ESP_LOGI
#define ESP_LOGI(tag, format, ...) 

// IO定义
#define DEV_POWER_CTRL        GPIO_NUM_5
#define BATTERY_ADC_GPIO      GPIO_NUM_6
#define BATTERY_ADC_CHAN      ADC1_CHANNEL_6  // GPIO6 对应 ADC1_CH6
#define ADC_SAMPLES    64               // 采样次数

class BatteryManager {
public:
    static BatteryManager* Instance() {
        static BatteryManager instance;
        return &instance;
    }

    // 初始化函数
    void init();
    
    // 获取电池电量
    float getBatteryLevel();

    // 获取电池电量
    int getBatteryLevelInt();

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    float batteryLevel = 0.0f;
    bool powerEnabled = false;

    // 电源管理锁（防止light sleep）
    esp_pm_lock_handle_t s_pm_lock;
};

#endif