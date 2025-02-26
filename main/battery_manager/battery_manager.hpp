#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"

// IO定义
#define BATTERY_POWER_CTRL    GPIO_NUM_17  // 控制电池开关
#define USB_CONNECT_DET       GPIO_NUM_18   // 检测USB连接
#define ADC1_CHAN      ADC1_CHANNEL_6  // GPIO7 对应 ADC1_CH6

#define ADC_ATTEN      ADC_ATTEN_DB_12  // 12dB衰减，量程0-3.3V
#define ADC_WIDTH      ADC_WIDTH_BIT_12 // 12位分辨率
#define ADC_SAMPLES    64               // 采样次数
#define LOWLEST_VOLTAGE 3.32

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
        return gpio_get_level(USB_CONNECT_DET) == 1;
    }

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    float batteryLevel = 0.0f;
    bool powerEnabled = false;
};

#endif