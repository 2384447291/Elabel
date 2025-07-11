#ifndef __BATTERY_MANAGER_HPP__
#define __BATTERY_MANAGER_HPP__

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "global_message.h"
#include "global_time.h"
#include "esp_log.h"
#include "control_driver.hpp"
#include "esp_adc_cal.h"
#include "esp_adc/adc_cali.h"
#include "global_draw.h"
#include "freertos/timers.h"
#include "global_message.h"
#include "codec.hpp"
#include "ssd1680.h"
#include "ElabelController.hpp"
#include "control_driver.hpp"
#include "esp_adc/adc_oneshot.h"

// IO定义
#define DEV_POWER_CTRL        GPIO_NUM_5
#define BATTERY_ADC_CHAN      ADC_CHANNEL_6  // GPIO6 对应 ADC1_CH6
#define SAMPLE_RATE_TIMES     20

#ifdef R01B_TEST
#define USB_CONNECT_GPIO GPIO_NUM_3
#elif defined(R01C_TEST)
#define USB_CONNECT_GPIO GPIO_NUM_3
#endif

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

    bool is_usb_connected(float battery_level);

    // 控制电池开关
    void setPowerState(bool enable);

    // 成员变量
    bool powerEnabled = false;

    // 电源管理锁（防止light sleep）
    esp_pm_lock_handle_t s_pm_lock;


    //adc读取参数
    adc_cali_handle_t cali_handle = NULL;

    void init_adc()
    {
        // 创建校准方案句柄
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .chan = BATTERY_ADC_CHAN,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "adc_cali_create_scheme_curve_fitting failed: %s", esp_err_to_name(ret));
            // 若返回 ESP_ERR_NOT_SUPPORTED，可继续使用原始读数
        } else {
            ESP_LOGI(TAG, "Calibration scheme created");
        }

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(ControlDriver::Instance()->adc_handle, BATTERY_ADC_CHAN, &chan_cfg));    
    }
};

#endif