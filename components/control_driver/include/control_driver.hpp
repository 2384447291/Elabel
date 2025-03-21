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
#define DEVICE_BUTTON_1 GPIO_NUM_7
#define DEVICE_BUTTON_1_CHANNEL ADC1_CHANNEL_6
#define DEVICE_BUTTON_2 GPIO_NUM_5
#define DEVICE_BUTTON_2_CHANNEL ADC1_CHANNEL_4

#define DEVICE_BUTTON_34 GPIO_NUM_4
#define DEVICE_BUTTON_34_CHANNEL ADC1_CHANNEL_3

#define DEVICE_BUTTON_56 GPIO_NUM_2
#define DEVICE_BUTTON_56_CHANNEL ADC1_CHANNEL_1

#define DEVICE_BUTTON_78 GPIO_NUM_1  
#define DEVICE_BUTTON_78_CHANNEL ADC1_CHANNEL_0


//----------------------------------------------ControlDriver类定义----------------------------------------------//
class ControlDriver {
public:
    void init();

    static ControlDriver* Instance() {
        static ControlDriver instance;
        return &instance;
    }

    Button button1{"Button_1", 1000, 100};
    Button button2{"Button_2", 1000, 100};
    Button button3{"Button_3", 1000, 100};
    Button button4{"Button_4", 1000, 100};
    Button button5{"Button_5", 1000, 100};
    Button button6{"Button_6", 1000, 100};
    Button button7{"Button_7", 1000, 100};
    Button button8{"Button_8", 1000, 100};

    void register_all_button_callback(Callback::CallbackFunc callback);
    void unregister_button_callback(Callback::CallbackFunc callback);
private:
    static void controlPanelUpdateTask(void* parameters);
};
//----------------------------------------------ControlDriver类定义----------------------------------------------//
#endif 