#include "encoder.hpp"

#define TAG "ENCODER"

// Encoder类实现
Encoder::Encoder(gpio_num_t _Encoder_gpio_A, gpio_num_t _Encoder_gpio_B, const char* buttonName) 
    : Encoder_gpio_A(_Encoder_gpio_A), Encoder_gpio_B(_Encoder_gpio_B),
    name(buttonName), value(0), 
    aTriggered(false), lastB(0), 
    lastRotationTime(0), rotationCount(0)
    {
    
    // 配置A引脚（中断引脚）
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << Encoder_gpio_A;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // 配置B引脚（状态读取引脚）
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << Encoder_gpio_B;
    gpio_config(&io_conf);
    gpio_isr_handler_add(Encoder_gpio_A, encoderIsrHandler, this);
}

void IRAM_ATTR Encoder::encoderIsrHandler(void* arg) {
    Encoder* encoder = static_cast<Encoder*>(arg);
    bool aLevel = gpio_get_level(encoder->Encoder_gpio_A);
    
    if (aLevel) {  // A引脚上升沿
        encoder->lastB = gpio_get_level(encoder->Encoder_gpio_B);
        encoder->aTriggered = true;
    } else if (encoder->aTriggered) {  // A引脚下降沿且之前有上升沿
        bool bLevel = gpio_get_level(encoder->Encoder_gpio_B);
        if (bLevel == 0 && encoder->lastB == 1) {
            encoder->value++;  // 顺时针
        } else if (bLevel == 1 && encoder->lastB == 0) {
            encoder->value--;  // 逆时针
        }
        encoder->aTriggered = false;
    }
}

void Encoder::handle() {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    int currentValue = value;
    static int lastValue = currentValue;
    
    // 检测旋转方向和计数
    if (currentValue != lastValue) {
        if (currentTime - lastRotationTime > ROTATION_TIMEOUT) {
            rotationCount = 0;
        }
        
        if (currentValue > lastValue) {
            rotationCount++;
            if (rotationCount >= ROTATION_THRESHOLD) {
                EncoderRightCircle.trigger();
                ESP_LOGI(TAG, "%s right circle", name);
                rotationCount = 0;
            }
        } else {
            rotationCount--;
            if (rotationCount <= -ROTATION_THRESHOLD) {
                EncoderLeftCircle.trigger();
                ESP_LOGI(TAG, "%s left circle", name);
                rotationCount = 0;
            }
        }
        
        lastRotationTime = currentTime;
        lastValue = currentValue;
    }
}