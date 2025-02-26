#ifndef CONTROL_DRIVER_HPP
#define CONTROL_DRIVER_HPP

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer.hpp"
#define EC56_GPIO_A GPIO_NUM_37
#define EC56_GPIO_B GPIO_NUM_38
#define DEVICE_UPDATE_PERIOD 20
#define DEVICE_DOWN_BUTTON GPIO_NUM_39
#define DEVICE_UP_BUTTON GPIO_NUM_6
#define DEVICE_POWER_BUTTON GPIO_NUM_41

//----------------------------------------------定义回调接口----------------------------------------------//
class Callback {
public:
    typedef void (*CallbackFunc)();
    static constexpr size_t MAX_CALLBACKS = 10;

    Callback(const char* name) : tag(name), callback_count(0) {
        for(size_t i = 0; i < MAX_CALLBACKS; i++) {
            callbacks[i] = nullptr;
        }
    }

    // 注册回调
    void registerCallback(CallbackFunc callback) {
        if (!callback) {
            // ESP_LOGE(tag, "Invalid callback");
            return;
        }

        if (callback_count >= MAX_CALLBACKS) {
            // ESP_LOGW(tag, "Callback array is full");
            return;
        }

        // 检查是否已存在
        for (size_t i = 0; i < callback_count; i++) {
            if (callbacks[i] == callback) {
                // ESP_LOGW(tag, "Callback already registered");
                return;
            }
        }

        callbacks[callback_count++] = callback;
        // ESP_LOGI(tag, "Callback registered successfully");
    }

    // 注销回调
    void unregisterCallback(CallbackFunc callback) {
        if (!callback) {
            // ESP_LOGE(tag, "Invalid callback");
            return;
        }

        for (size_t i = 0; i < callback_count; i++) {
            if (callbacks[i] == callback) {
                // 移动后面的回调前移
                for (size_t j = i; j < callback_count - 1; j++) {
                    callbacks[j] = callbacks[j + 1];
                }
                callback_count--;
                callbacks[callback_count] = nullptr;
                // ESP_LOGI(tag, "Callback unregistered successfully");
                return;
            }
        }

        // ESP_LOGW(tag, "Callback not found");
    }

    // 触发所有回调
    void trigger() {
        for (size_t i = 0; i < callback_count; i++) {
            if (callbacks[i]) {
                callbacks[i]();
            }
        }
    }

private:
    const char* tag;
    CallbackFunc callbacks[MAX_CALLBACKS];
    size_t callback_count;
};
//----------------------------------------------定义回调接口----------------------------------------------//


//----------------------------------------------Button类定义----------------------------------------------//
class Button {
public:
    enum class State {
        IDLE,               // 空闲状态
        PRESSED,            // 按下状态
        WAIT_RELEASE,       // 等待释放状态
        WAIT_DOUBLE_CLICK   // 等待双击状态
    };

    Button(gpio_num_t gpio, const char* buttonName, uint32_t debounceDelay, uint32_t longPressTime);
    void handle();
    static void IRAM_ATTR buttonIsrHandler(void* arg);

private:
    gpio_num_t gpioNum;
    const char* name;
    uint32_t debounceDelay;
    uint32_t longPressTime;
    
    // 状态相关
    State state;
    uint32_t pressTime;
    uint32_t lastPressTime;
    uint8_t clickCount;
    volatile bool isrTriggered;
};
//----------------------------------------------Button类定义----------------------------------------------//



//----------------------------------------------Encoder类定义----------------------------------------------//
class Encoder {
public:
    Encoder();
    void handle();
    static void IRAM_ATTR encoderIsrHandler(void* arg);
    int getValue() const { return value; }
    void setValue(int val) { value = val; }
    void reset() { value = 0; }

private:
    static constexpr uint32_t ROTATION_THRESHOLD = 2;  // 旋转计数阈值
    static constexpr uint32_t ROTATION_TIMEOUT = 600;  // 旋转超时时间(ms)

    volatile int value;
    volatile bool aTriggered;
    volatile uint8_t lastB;
    uint32_t lastRotationTime;
    int rotationCount;
};
//----------------------------------------------Encoder类定义----------------------------------------------//

//----------------------------------------------ControlDriver类定义----------------------------------------------//
class ControlDriver {
public:
    void init();

    static ControlDriver* Instance() {
        static ControlDriver instance;
        return &instance;
    }

    int getEncoderValue() const { return encoder.getValue(); }
    void resetEncoderValue() { encoder.reset(); }
    void setEncoderValue(int value) { encoder.setValue(value); }

    // 蜂鸣器控制接口
    Buzzer& getBuzzer() { return buzzer; }
    void buzzerStart(uint32_t frequency) { buzzer.start(frequency); }
    void buzzerStop() { buzzer.stop(); }
    void buzzerPlayNote(uint32_t frequency, uint32_t duration_ms) { buzzer.playNote(frequency, duration_ms); }
    bool isBuzzerBusy() const { return buzzer.isBusy(); }

    Callback ButtonDownShortPress{"Button_DOWN_SHORT"};
    Callback ButtonDownLongPress{"Button_DOWN_LONG"};
    Callback ButtonUpShortPress{"Button_UP_SHORT"};
    Callback ButtonUpLongPress{"Button_UP_LONG"};
    Callback EncoderRightCircle{"Encoder_RIGHT"};
    Callback EncoderLeftCircle{"Encoder_LEFT"};
    Callback ButtonDownDoubleclick{"Button_DOWN_DOUBLE"};
    Callback ButtonUpDoubleclick{"Button_UP_DOUBLE"};
    Callback ButtonPowerDoubleclick{"Button_POWER_DOUBLE"};
    Callback ButtonPowerShortPress{"Button_POWER_SHORT"};
    Callback ButtonPowerLongPress{"Button_POWER_LONG"};

private:
    static void controlPanelUpdateTask(void* parameters);

    Button buttonDown{DEVICE_DOWN_BUTTON, "Button_DOWN", 20, 800};
    Button buttonUp{DEVICE_UP_BUTTON, "Button_UP", 20, 800};
    Button buttonPower{DEVICE_POWER_BUTTON, "Button_POWER", 20, 800};
    Encoder encoder;
    Buzzer buzzer;
};
//----------------------------------------------ControlDriver类定义----------------------------------------------//
#endif 