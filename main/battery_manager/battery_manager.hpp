#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/adc.h"

// IO定义
#define BATTERY_POWER_CTRL    GPIO_NUM_15  // 控制电池开关
#define USB_CONNECT_DET       GPIO_NUM_16   // 检测USB连接
#define USB_CONNECT_CHANNEL   ADC2_CHANNEL_5
#define ADC1_CHAN      ADC1_CHANNEL_5  // GPIO6 对应 ADC1_CH5

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
        int adc_value; 
        adc2_get_raw(USB_CONNECT_CHANNEL, ADC_WIDTH_BIT_12, &adc_value);
        float voltage = adc_value * 3.3 / 4095;
        ESP_LOGI("USB","Voltage is %f",voltage);
        if(voltage < 2.5) return false;
        else return true;
    }

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    float batteryLevel = 0.0f;
    bool powerEnabled = false;
};

#endif