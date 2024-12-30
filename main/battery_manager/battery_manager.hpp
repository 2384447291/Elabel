#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"

// IO定义
#define BATTERY_POWER_CTRL    GPIO_NUM_13  // 控制电池开关
#define BATTERY_CHARGING_DET  GPIO_NUM_48  // 充电状态检测
#define BATTERY_ADC_DET      GPIO_NUM_11  // 电池电压检测ADC

class BatteryManager {
public:
    static BatteryManager* Instance() {
        static BatteryManager instance;
        return &instance;
    }

    // 初始化函数
    void init();

    //更新函数
    static void battery_update_task(void* parameters);
    
    // 获取电池电量
    float getBatteryLevel();

    // 获取充电状态
    bool isCharging() { return chargingStatus; }

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    float batteryLevel = 0.0f;
    bool chargingStatus = false;
    bool powerEnabled = false;
};

#endif