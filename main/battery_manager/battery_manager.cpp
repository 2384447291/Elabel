#include "battery_manager.hpp"
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
    //开启外设电源
    setPowerState(true);


    // 初始化ADC
    init_adc();


    // 初始化电源管理
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
    //保持电源关闭
    gpio_hold_dis(DEV_POWER_CTRL);
    gpio_set_level(DEV_POWER_CTRL, enable ? 1 : 0);
    gpio_hold_en(DEV_POWER_CTRL);
    powerEnabled = enable;
    ESP_LOGE(TAG, "Battery power state: %s", enable ? "ON" : "OFF");
}

float BatteryManager::getBatteryLevel() {
    int raw = 0;
    int sum_raw = 0;
    int voltage = 0;
    for(int i = 0; i < SAMPLE_RATE_TIMES; i++)
    {
        ControlDriver::Instance()->lock_adc();
        adc_oneshot_read(ControlDriver::Instance()->adc_handle, BATTERY_ADC_CHAN, &raw);
        ControlDriver::Instance()->release_adc();
        sum_raw += raw;
    }
    raw = sum_raw / SAMPLE_RATE_TIMES;

    if (cali_handle) 
    {
        esp_err_t r2 = adc_cali_raw_to_voltage(cali_handle, raw, &voltage);
        if (r2 != ESP_OK) 
        {
            ESP_LOGE(TAG, "adc_cali_raw_to_voltage error: %s", esp_err_to_name(r2));
        }
    } 
    else 
    {
        ESP_LOGE(TAG, "Raw ADC (no calibration): %d", raw);
    }

    float actual_voltage = (float)voltage * 2.0f / 1000.0f; // 转换为V
    
    ESP_LOGI(TAG, "Battery voltage: %.3fV", actual_voltage);
    return actual_voltage;
}

bool BatteryManager::is_usb_connected(float battery_level) 
{
    #ifdef R01A_TEST
    if(battery_level < 1.0f || battery_level > 4.5f)
    {
        return true;
    }else
    {
        return false;
    }
    #elif defined(R01B_TEST)
    return gpio_get_level(USB_CONNECT_GPIO) == 1;
    #endif
}

int BatteryManager::getBatteryLevelInt() 
{
    float battery_level = getBatteryLevel();
    if(is_usb_connected(battery_level))
    {
        return -1;
    }
    else 
    {
        if(battery_level > 4.2f)
        {
            return 100;
        }
        else if(battery_level > 3.8f)
        {
            return 75;
        }
        else if(battery_level > 3.5f)
        {
            return 50;
        }
        else if(battery_level > 3.3f)
        {
            return 25;
        }
        else
        {
            return 0;
        }
    }
}
