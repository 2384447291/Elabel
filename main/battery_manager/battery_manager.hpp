#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"

// IO定义
#define BATTERY_POWER_CTRL    GPIO_NUM_13  // 控制电池开关
#define BATTERY_CHARGING_DET  GPIO_NUM_48  // 充电状态检测
#define BATTERY_FULL_POWER_DET  GPIO_NUM_45  // 是否为满电
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

    //检测是否有usb连接
    bool isUsbConnected()
    {
        if(chargingStatus && !fullPowerStatus)
        {
            return true;
        }
        else if(!chargingStatus && fullPowerStatus)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    float batteryLevel = 0.0f;
    bool chargingStatus = false;
    bool fullPowerStatus = false;
    bool powerEnabled = false;
};

#endif