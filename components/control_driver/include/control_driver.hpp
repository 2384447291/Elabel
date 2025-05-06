#ifndef CONTROL_DRIVER_HPP
#define CONTROL_DRIVER_HPP

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer.hpp"
#include "callback.hpp"
#include "encoder.hpp"
#include "button.hpp"

// 按键定义
#define DEVICE_BUTTON_123 GPIO_NUM_5
#define DEVICE_BUTTON_123_CHANNEL ADC1_CHANNEL_5
#define DEVICE_BUTTON_4 GPIO_NUM_4
#define DEVICE_BUTTON_4_CHANNEL ADC1_CHANNEL_4

#define DEVICE_BUTTON_567 GPIO_NUM_2
#define DEVICE_BUTTON_567_CHANNEL ADC1_CHANNEL_2

#define DEVICE_BUTTON_8 GPIO_NUM_3
#define DEVICE_BUTTON_8_CHANNEL ADC1_CHANNEL_3

//----------------------------------------------ControlDriver类定义----------------------------------------------//
class ControlDriver {
public:
    void init();

    static ControlDriver* Instance() {
        static ControlDriver instance;
        return &instance;
    }

    Button button1{"Button_1", 1000};
    Button button2{"Button_2", 1000};
    Button button3{"Button_3", 1000};
    Button button4{"Button_4", 1000};
    Button button5{"Button_5", 1000};
    Button button6{"Button_6", 1000};
    Button button7{"Button_7", 1000};
    Button button8{"Button_8", 1000};

    Button_pair_3 button_pair_123{DEVICE_BUTTON_123, DEVICE_BUTTON_123_CHANNEL, &button1, &button2, &button3};
    Button_pair_1 button_pair_4{DEVICE_BUTTON_4, DEVICE_BUTTON_4_CHANNEL, &button4};
    Button_pair_3 button_pair_567{DEVICE_BUTTON_567, DEVICE_BUTTON_567_CHANNEL, &button5, &button6, &button7};
    Button_pair_1 button_pair_8{DEVICE_BUTTON_8, DEVICE_BUTTON_8_CHANNEL, &button8};

    void register_all_button_callback(Callback::CallbackFunc callback);
    void unregister_button_callback(Callback::CallbackFunc callback);

    void start_button_check_task();
    void stop_button_check_task();

private:
    static void button_check_task(void* parameters);
    TaskHandle_t button_check_task_handle;
};
//----------------------------------------------ControlDriver类定义----------------------------------------------//
#endif 