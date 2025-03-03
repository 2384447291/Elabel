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

#define DEVICE_UPDATE_PERIOD 20

// 按键定义
#define DEVICE_BUTTON_1 GPIO_NUM_6
#define DEVICE_BUTTON_2 GPIO_NUM_16

#define DEVICE_BUTTON_34 GPIO_NUM_5
#define DEVICE_BUTTON_34_CHANNEL ADC1_CHANNEL_4

#define DEVICE_BUZZER GPIO_NUM_1


//----------------------------------------------ControlDriver类定义----------------------------------------------//
class ControlDriver {
public:
    void init();

    static ControlDriver* Instance() {
        static ControlDriver instance;
        return &instance;
    }

    Button button1{"Button_1", 800, 200};
    Button button2{"Button_2", 800, 200};
    Button button3{"Button_3", 800, 200};
    Button button4{"Button_4", 800, 200};
    Button button5{"Button_5", 800, 200};
    Button button6{"Button_6", 800, 200};
    Button button7{"Button_7", 800, 200};
    Button button8{"Button_8", 800, 200};

    Buzzer buzzer{DEVICE_BUZZER, "Buzzer"};

    void buzzerStart(uint32_t frequency) { buzzer.start(frequency); }
    void buzzerStop() { buzzer.stop(); }
    void buzzerPlayNote(uint32_t frequency, uint32_t duration_ms) { buzzer.playNote(frequency, duration_ms); }
    bool isBuzzerBusy() const { return buzzer.isBusy(); }
private:
    static void controlPanelUpdateTask(void* parameters);
};
//----------------------------------------------ControlDriver类定义----------------------------------------------//
#endif 