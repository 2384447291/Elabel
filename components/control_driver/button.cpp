#include "button.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "callback.hpp"
#include "driver/adc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"

#define TAG "BUTTON"
#define R0 20.0
#define R1 20.0
#define R2 10.0
#define R3 3.0

#define V_BUTTON_0 3.3
#define V_BUTTON_1 (3.3*(R0/(R0+R1)))
#define V_BUTTON_2 (3.3*(R0/(R0+R2)))
#define V_BUTTON_3 (3.3*(R0/(R0+R3)))
#define ERROR_RANGE 0.1

#define STATE_DURATION_MS 40

// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

Button::Button(const char* buttonName, uint32_t longPressTime)
    : name(buttonName),
      longPressTime(longPressTime),
      state(State::IDLE),
      pressTime(0),
      lastPressTime(0),
      isPressed(false),
      isrTriggered(false) {
}

void Button::handle() {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (isrTriggered) {
        isrTriggered = false;
        switch (state) {
            case State::IDLE:
                if (isPressed) {  // 按下
                    pressTime = currentTime;
                    state = State::PRESSED;
                }
                break;
            case State::PRESSED:
                if (!isPressed) {  // 释放
                    CallbackShortPress.trigger();
                    ESP_LOGI(TAG, "%s short pressed", name);
                    state = State::IDLE;
                }
            case State::WAITING_RELEASE:
                if (!isPressed) {
                    state = State::IDLE;
                }
                break;
        }
    }

    if (state == State::PRESSED && currentTime - pressTime >= longPressTime) {
        CallbackLongPress.trigger();
        ESP_LOGI(TAG, "%s long pressed, duration: %d ms", name, (int)(currentTime - pressTime));
        state = State::WAITING_RELEASE;
    }
}


//----------------------------------------------单个IO控制三个按钮----------------------------------------------//
void Button_pair_3::update() 
{
    int adc_value = adc1_get_raw(adc1_chan);
    float voltage = adc_value / 1000.0f;

    // 临时状态变量,默认都是0
    bool temp_button_state[3] = {false, false, false};
    // 判断当前状态
    if (voltage > V_BUTTON_0 - ERROR_RANGE && voltage < V_BUTTON_0 + ERROR_RANGE) {  // 都未按下
        temp_button_state[0] = true;
        temp_button_state[1] = false;
        temp_button_state[2] = false;
    } else if (voltage > V_BUTTON_1 - ERROR_RANGE && voltage < V_BUTTON_1 + ERROR_RANGE) {  // 按下按键1
        temp_button_state[0] = false;
        temp_button_state[1] = true;
        temp_button_state[2] = false;
    } else if (voltage > V_BUTTON_2 - ERROR_RANGE && voltage < V_BUTTON_2 + ERROR_RANGE) {  // 按下按键2
        temp_button_state[0] = false;
        temp_button_state[1] = false;
        temp_button_state[2] = true;
    }

    // 检查状态是否发生变化
    if (temp_button_state[0] != current_button_state[0] || temp_button_state[1] != current_button_state[1] || temp_button_state[2] != current_button_state[2]) 
    {
        // 状态发生变化，重置计时器
        state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        for (int i = 0; i < 3; i++) {
            current_button_state[i] = temp_button_state[i];
        }
    //如果没有发生变化，则检查持续时间
    } 
    else 
    {
        // 状态未变化，检查持续时间
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - state_start_time >= STATE_DURATION_MS) 
        {
            // 持续时间达到阈值，触发状态变化
            for (int i = 0; i < 3; i++) 
            {
                if (current_button_state[i] != last_button_state[i]) 
                {
                    button[i]->isPressed = current_button_state[i];
                    button[i]->isrTriggered = true;
                    last_button_state[i] = current_button_state[i];
                }
            }
        }
    }
}

Button_pair_3::Button_pair_3(gpio_num_t _gpio, adc1_channel_t _adc1_chan, Button* _button0, Button* _button1, Button* _button2)
{    
    button[0] = _button0;
    button[1] = _button1;
    button[2] = _button2;
    gpio = _gpio;
    adc1_chan = _adc1_chan;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc1_chan, ADC_ATTEN_DB_12);
}

void Button_pair_3::clear_state()
{
    for (int i = 0; i < 3; i++) {
        current_button_state[i] = false;
        last_button_state[i] = false;
        button[i]->clear_state();
    }
}

//----------------------------------------------单个IO控制三个按钮----------------------------------------------//



//----------------------------------------------单个IO控制一个按钮----------------------------------------------//


void Button_pair_1::update() 
{
    int adc_value = adc1_get_raw(adc1_chan);
    float voltage = adc_value / 1000.0f;

    // 按下是高电平
    if(voltage > 2.0f && button->isPressed == false)
    {
        button->isrTriggered = true;
        button->isPressed = true;
    }
    else if(voltage < 1.0f && button->isPressed == true)
    {
        button->isrTriggered = true;
        button->isPressed = false;
    }
}

Button_pair_1::Button_pair_1(gpio_num_t _gpio, adc1_channel_t _adc1_chan, Button* _button)
{    
    this->button = _button;
    gpio = _gpio;
    adc1_chan = _adc1_chan;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc1_chan, ADC_ATTEN_DB_12);
}

void Button_pair_1::clear_state()
{
    button->clear_state();
}



//----------------------------------------------单个IO控制一个按钮----------------------------------------------//


//----------------------------------------------单个IO控制四个按钮----------------------------------------------//
void Button_pair_4::update() 
{
    int adc_value = adc1_get_raw(adc1_chan);
    float voltage = adc_value / 1000.0f;

    // 临时状态变量,默认都是0
    bool temp_button_state[4] = {false, false, false, false};
    // 判断当前状态
    if (voltage > V_BUTTON_0 - ERROR_RANGE && voltage < V_BUTTON_0 + ERROR_RANGE) {  // 都未按下
        temp_button_state[0] = true;
        temp_button_state[1] = false;
        temp_button_state[2] = false;
        temp_button_state[3] = false;
    } else if (voltage > V_BUTTON_1 - ERROR_RANGE && voltage < V_BUTTON_1 + ERROR_RANGE) {  // 按下按键1
        temp_button_state[0] = false;
        temp_button_state[1] = true;
        temp_button_state[2] = false;
        temp_button_state[3] = false;
    } else if (voltage > V_BUTTON_2 - ERROR_RANGE && voltage < V_BUTTON_2 + ERROR_RANGE) {  // 按下按键2
        temp_button_state[0] = false;
        temp_button_state[1] = false;
        temp_button_state[2] = true;
        temp_button_state[3] = false;
    } else if (voltage > V_BUTTON_3 - ERROR_RANGE && voltage < V_BUTTON_3 + ERROR_RANGE) {  // 按下按键3
        temp_button_state[0] = false;
        temp_button_state[1] = false;
        temp_button_state[2] = false;
        temp_button_state[3] = true;
    }

    // 检查状态是否发生变化
    if (temp_button_state[0] != current_button_state[0] || temp_button_state[1] != current_button_state[1] || temp_button_state[2] != current_button_state[2] || temp_button_state[3] != current_button_state[3]) 
    {
        // 状态发生变化，重置计时器
        state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        for (int i = 0; i < 4; i++) {
            current_button_state[i] = temp_button_state[i];
        }
    //如果没有发生变化，则检查持续时间
    } 
    else 
    {
        // 状态未变化，检查持续时间
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - state_start_time >= STATE_DURATION_MS) 
        {
            // 持续时间达到阈值，触发状态变化
            for (int i = 0; i < 4; i++) 
            {
                if (current_button_state[i] != last_button_state[i]) 
                {
                    button[i]->isPressed = current_button_state[i];
                    button[i]->isrTriggered = true;
                    last_button_state[i] = current_button_state[i];
                }
            }
        }
    }
}

Button_pair_4::Button_pair_4(gpio_num_t _gpio, adc1_channel_t _adc1_chan, Button* _button0, Button* _button1, Button* _button2, Button* _button3)
{    
    button[0] = _button0;
    button[1] = _button1;
    button[2] = _button2;
    button[3] = _button3;
    gpio = _gpio;
    adc1_chan = _adc1_chan;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc1_chan, ADC_ATTEN_DB_12);
}

void Button_pair_4::clear_state()
{
    for (int i = 0; i < 4; i++) {
        current_button_state[i] = false;
        last_button_state[i] = false;
        button[i]->clear_state();
    }
}

//----------------------------------------------单个IO控制四个按钮----------------------------------------------//