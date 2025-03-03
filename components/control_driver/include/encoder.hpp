#ifndef ENCODER_HPP
#define ENCODER_HPP

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "callback.hpp"

class Encoder {
public:
    Encoder(gpio_num_t _Encoder_gpio_A, gpio_num_t _Encoder_gpio_B, const char* buttonName);
    void handle();
    static void IRAM_ATTR encoderIsrHandler(void* arg);
    int getValue() const { return value; }
    void setValue(int val) { value = val; }
    void reset() { value = 0; }
    Callback EncoderRightCircle{"Encoder_RIGHT"};
    Callback EncoderLeftCircle{"Encoder_LEFT"};

private:
    gpio_num_t Encoder_gpio_A;
    gpio_num_t Encoder_gpio_B;
    const char* name;
    int value;
    bool aTriggered;
    uint8_t lastB;
    uint32_t lastRotationTime;
    int rotationCount;
    static constexpr uint32_t ROTATION_THRESHOLD = 2;  // 旋转计数阈值
    static constexpr uint32_t ROTATION_TIMEOUT = 600;  // 旋转超时时间(ms)
};

#endif