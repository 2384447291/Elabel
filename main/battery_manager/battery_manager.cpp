#include "battery_manager.hpp"
#include "global_time.h"
#include "esp_log.h"
#include "control_driver.hpp"
#include "driver/adc.h"
#include "driver/adc_common.h"
#include "esp_adc_cal.h"
#include "global_draw.h"
#include "freertos/timers.h"
#define TAG "BATTERY_MANAGER"

#define ADC2_CHAN      ADC2_CHANNEL_0  // GPIO11 对应 ADC2_CH0
#define ADC_ATTEN      ADC_ATTEN_DB_12  // 12dB衰减，量程0-3.3V
#define ADC_WIDTH      ADC_WIDTH_BIT_12 // 12位分辨率
#define ADC_SAMPLES    64               // 采样次数
#define LOWLEST_VOLTAGE 3.32

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
    lv_scr_load(ui_ShutdownScreen);
    // char* time_str = get_time_str();
    // set_text(ui_ShutdownGuide, time_str);
    //释放lvgl
    release_lvgl();
    // 播放关机音乐
    ControlDriver::Instance()->getBuzzer().playDownMusic();

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

void IRAM_ATTR chargingIsrHandler(void* arg) {
    BatteryManager* manager = static_cast<BatteryManager*>(arg);
    // 获取当前电平状态
    int level = gpio_get_level(BATTERY_CHARGING_DET);
    // 高电平表示未充电，低电平表示充电
    manager->chargingStatus = (level == 0);
}

void BatteryManager::battery_update_task(void* parameters) {
    while (true) {
        //1s检查一次电量
        vTaskDelay(5000 / portTICK_PERIOD_MS);   
        if(BatteryManager::Instance()->getBatteryLevel() <= LOWLEST_VOLTAGE) {
            lock_lvgl();
            // char ShutdownGuide[100];
            // sprintf(ShutdownGuide, "Battery low is  %.3fV",BatteryManager::Instance()->getBatteryLevel());
            set_text(ui_ShutdownGuide, "No Battery Please Charge");
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

    // 配置充电检测引脚（输入，带中断）
    io_conf.pin_bit_mask = (1ULL << BATTERY_CHARGING_DET);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // 上升沿和下降沿都触发中断
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // 初始化ADC2
    adc2_config_channel_atten(ADC2_CHAN, ADC_ATTEN);
    
    // 特性曲线校准
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN, ADC_WIDTH, 1100, &adc_chars);

    // 安装GPIO中断服务
    gpio_install_isr_service(0);
    // 添加中断处理程序
    gpio_isr_handler_add(BATTERY_CHARGING_DET, chargingIsrHandler, this);

    // 初始化充电状态
    chargingStatus = (gpio_get_level(BATTERY_CHARGING_DET) == 0);

    // 初始化完成后默认开启电源
    setPowerState(true);
    ESP_LOGI(TAG, "Battery manager initialized, initial charging status: %s", 
             chargingStatus ? "charging" : "not charging");
    ControlDriver::Instance()->ButtonPowerLongPress.registerCallback(power_off_system);

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
    int raw = 0;
    
    // 多次采样取平均值
    for (int i = 0; i < ADC_SAMPLES; i++) {
        if (adc2_get_raw(ADC2_CHAN, ADC_WIDTH, &raw) == ESP_OK) {
            adc_reading += raw;
        }
    }
    adc_reading /= ADC_SAMPLES;
    
    // 将ADC读数转换为实际电压值（mV）
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    
    // 由于使用分压电路，这里需要根据实际分压比例计算真实电池电压
    // 假设使用100K和100K的分压电阻，则实际电压为ADC读数的2倍
    float actual_voltage = voltage * 2.0f / 1000.0f;  // 转换为V
    
    //4.18-->4.21有点小误差
    ESP_LOGI(TAG, "Battery voltage: %.3fV", actual_voltage);
    batteryLevel = actual_voltage;
    return actual_voltage;
}
