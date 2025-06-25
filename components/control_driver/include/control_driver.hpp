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
#include "global_message.h"

// 按键定义
#ifdef R01A_TEST
#define DEVICE_BUTTON_1234 GPIO_NUM_4
#define DEVICE_BUTTON_1234_CHANNEL ADC_CHANNEL_4

#define DEVICE_BUTTON_567 GPIO_NUM_2
#define DEVICE_BUTTON_567_CHANNEL ADC_CHANNEL_2

#define DEVICE_BUTTON_8 GPIO_NUM_3
#define DEVICE_BUTTON_8_CHANNEL ADC_CHANNEL_3
#elif defined(R01B_TEST)
#define DEVICE_BUTTON_1234 GPIO_NUM_4
#define DEVICE_BUTTON_1234_CHANNEL ADC_CHANNEL_4
#define DEVICE_BUTTON_5678 GPIO_NUM_2
#define DEVICE_BUTTON_5678_CHANNEL ADC_CHANNEL_2
#endif

class Button_Press_together{
public:
    Button_Press_together(Button* _button1, Button* _button2, const char* _name) : button1(_button1), button2(_button2), name(_name) {}
    Button* button1;
    Button* button2;
    const char* name;

    Callback Togetherlongpress{"Togetherlongpress"};

    bool is_trigger = false;

    void update()
    {
        if(button1->state == Button::State::WAITING_RELEASE && button2->state == Button::State::WAITING_RELEASE)
        {
            if(is_trigger == false)
            {
                Togetherlongpress.trigger();
                ESP_LOGI("Button", "%s triggered", name);
                is_trigger = true;
            }
        }
        else
        {
            is_trigger = false;
        }
    }
    void clear_state() 
    {
        is_trigger = false;
    }
};

//----------------------------------------------ControlDriver类定义----------------------------------------------//
class ControlDriver {
public:
    void init();

    static ControlDriver* Instance() {
        static ControlDriver instance;
        return &instance;
    }

    //adc读取参数
    adc_oneshot_unit_handle_t adc_handle = NULL;
    SemaphoreHandle_t xadcSemaphore;

    void lock_adc()
    {
        while(true)
        {
            if (pdTRUE == xSemaphoreTake(xadcSemaphore, portMAX_DELAY)) break;
        }
    }
    void release_adc()
    {
        xSemaphoreGive(xadcSemaphore);
    }

    Button button1{"Button_1", 1000};
    Button button2{"Button_2", 1000};
    Button button3{"Button_3", 1000};
    Button button4{"Button_4", 1000};
    Button button5{"Button_5", 1000};
    Button button6{"Button_6", 1000};
    Button button7{"Button_7", 1000};
    Button button8{"Button_8", 1000};

#ifdef R01A_TEST
    Button_pair_4 button_pair_1234;
    Button_pair_3 button_pair_567;
    Button_pair_1 button_pair_8;
#elif defined(R01B_TEST)
    Button_pair_4 button_pair_1234;
    Button_pair_4 button_pair_5678;
#endif

    Button_Press_together button_press_together_48{&button4, &button8, "button_press_together_48"};
    Button_Press_together button_press_together_15{&button1, &button5, "button_press_together_15"};

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