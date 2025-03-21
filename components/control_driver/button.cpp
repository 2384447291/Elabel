#include "button.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "callback.hpp"
#include "driver/adc.h"

#define TAG "BUTTON"

#define V_00 3.3
#define V_01 1.56
#define V_10 1.025
#define V_11 0.765

// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

Button::Button(const char* buttonName, uint32_t longPressTime, uint32_t doubleClickTime)
    : name(buttonName),
      longPressTime(longPressTime),
      doubleClickTime(doubleClickTime),
      state(State::IDLE),
      pressTime(0),
      lastPressTime(0),
      isPressed(false),
      isrTriggered(false) {
}

void Button::handle() {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    // 如果发生了GPIO状态变化，更新状态机
    if (isrTriggered) 
    {
        isrTriggered = false;  // 清除中断标志
        switch (state) 
        {
            case State::IDLE:
                if (isPressed) {  // 按下
                    pressTime = currentTime;
                    state = State::PRESSED;
                }
                break;

            case State::PRESSED:
                if (!isPressed) {  // 释放
                    if (currentTime - pressTime < longPressTime) {  // 未达到长按时间
                        CallbackShortPress.trigger();
                        ESP_LOGI(TAG, "%s short pressed", name);
                        state = State::IDLE;
                    }
                }
                break;

            case State::WAIT_RELEASE:
                if (!isPressed) {  // 长按释放
                    state = State::IDLE;
                }
                break;
        }
    }

    // 检查长按情况
    switch (state) {
        case State::IDLE:
            // 空闲状态不需要额外处理
            break;
            
        case State::PRESSED:
            if (isPressed && (currentTime - pressTime >= longPressTime)) {
                CallbackLongPress.trigger();
                ESP_LOGI(TAG, "%s long pressed, duration: %u ms", name, currentTime - pressTime);
                state = State::WAIT_RELEASE;
            }
            break;

        case State::WAIT_RELEASE:
            // 等待释放状态不需要额外处理
            break;
    }
}


//----------------------------------------------单个IO控制一个按钮----------------------------------------------//
typedef struct {
    Button* btn;
    gpio_num_t gpio;
    bool running;
} button_single_t;

typedef struct {
    Button* btn;
    adc1_channel_t adc_chan;
    bool running;
} button_single_adc1_t;

typedef struct {
    Button* btn;
    adc2_channel_t adc_chan;
    bool running;   
} button_single_adc2_t;

void button_single_task(void* arg) {
    button_single_t* pair = (button_single_t*)arg;

    // 记录上一次的状态
    bool last_btn_state = false;

    // 当前状态
    bool current_btn_state = false;

    while (pair->running) {
        current_btn_state = !gpio_get_level(pair->gpio);
        // 检查状态变化并触发中断
        if (current_btn_state != last_btn_state) {
            pair->btn->isPressed = current_btn_state;
            pair->btn->isrTriggered = true;
            // 更新上一次的状态
            last_btn_state = current_btn_state;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    delete pair;
    vTaskDelete(NULL);
}

void button_single_adc1_task(void* arg) {
    button_single_adc1_t* pair = (button_single_adc1_t*)arg;

    // 记录上一次的状态
    bool last_btn_state = false;

    // 当前状态
    bool current_btn_state = false;

    while (pair->running) {
        int adc_value = adc1_get_raw(pair->adc_chan);
        float voltage = adc_value * 3.3 / 4095; 

        // 判断当前状态
        if (voltage  > 3.0) {  
            current_btn_state = false;
        } 
        else if(voltage < 0.3){  
            current_btn_state = true;
        }

        // 检查状态变化并触发中断
        if (current_btn_state != last_btn_state) {
            pair->btn->isPressed = current_btn_state;
            pair->btn->isrTriggered = true;
            last_btn_state = current_btn_state;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }   

    delete pair;
    vTaskDelete(NULL);
}

void button_single_adc2_task(void* arg) {
    button_single_adc2_t* pair = (button_single_adc2_t*)arg;

    // 记录上一次的状态
    bool last_btn_state = false;    

    // 当前状态
    bool current_btn_state = false;

    while (pair->running) {
        int adc_value; 
        adc2_get_raw(pair->adc_chan, ADC_WIDTH_BIT_12, &adc_value); 
        float voltage = adc_value * 3.3 / 4095; 

        // 判断当前状态
        if (voltage  > 3.3 - 0.5) {  // 都未按下
            current_btn_state = false;
        } 
        else if(voltage < 3.3 - 0.5){  // 按下
            current_btn_state = true;
        }   

        // 检查状态变化并触发中断
        if (current_btn_state != last_btn_state) {
            pair->btn->isPressed = current_btn_state;
            pair->btn->isrTriggered = true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }   

    delete pair;
    vTaskDelete(NULL);
}

void Button::register_single_io(gpio_num_t gpio, Button* button)
{
    button_single_t* pair = new button_single_t{
        .btn = button,
        .gpio = gpio,
        .running = true
    };

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "button_gpio%d", gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    xTaskCreate(button_single_task, task_name, 2048, pair, 5, NULL);
}

// 重载函数 - ADC1 版本
void Button::register_single_io(gpio_num_t gpio, adc1_channel_t adc1_chan, Button* button)
{    
    button_single_adc1_t* pair = new button_single_adc1_t{
        .btn = button,
        .adc_chan = adc1_chan,
        .running = true
    };

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "button_adc_gpio%d", gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // ADC1 配置
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc1_chan, ADC_ATTEN_DB_12);

    xTaskCreate(button_single_adc1_task, task_name, 2048, pair, 5, NULL);
}

// 重载函数 - ADC2 版本
void Button::register_single_io(gpio_num_t gpio, adc2_channel_t adc2_chan, Button* button)
{    
    button_single_adc2_t* pair = new button_single_adc2_t{
        .btn = button,
        .adc_chan = adc2_chan,
        .running = true
    };

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "button_adc_gpio%d", gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // ADC2 配置
    adc2_config_channel_atten(adc2_chan, ADC_ATTEN_DB_12);

    xTaskCreate(button_single_adc2_task, task_name, 2048, pair, 5, NULL);
}
//----------------------------------------------单个IO控制一个按钮----------------------------------------------//


//----------------------------------------------单个IO控制两个按钮----------------------------------------------//
// 定义一个结构体来存储按键信息
typedef struct {
    Button* btn1;
    Button* btn2;
    gpio_num_t gpio;
    bool running;
    adc1_channel_t adc1_chan;
} button_adc1_pair_t;

typedef struct {
    Button* btn1;
    Button* btn2;
    gpio_num_t gpio;
    bool running;
    adc2_channel_t adc2_chan;
} button_adc2_pair_t;

void button_adc1_task(void* arg) {
    button_adc1_pair_t* pair = (button_adc1_pair_t*)arg;

    // 记录上一次的状态
    bool last_btn1_state = false;
    bool last_btn2_state = false;

    // 当前状态
    bool current_btn1_state = false;
    bool current_btn2_state = false;

    // 用于记录状态持续时间
    uint32_t state_start_time = 0;
    const uint32_t STATE_DURATION_MS = 50;  // 状态持续时间阈值

    while (pair->running) {
        // 单次采样
        int adc_value = adc1_get_raw(pair->adc1_chan);
        float voltage = adc_value * 3.3 / 4095;

        // 临时状态变量
        bool temp_btn1_state = false;
        bool temp_btn2_state = false;

        // 判断当前状态
        if (voltage > V_00 - 0.02 && voltage < V_00 + 0.02) {  // 都未按下
            temp_btn1_state = false;
            temp_btn2_state = false;
        } else if (voltage > V_01 - 0.02 && voltage < V_01 + 0.02) {  // 按下按键1
            temp_btn1_state = true;
            temp_btn2_state = false;
        } else if (voltage > V_10 - 0.1 && voltage < V_10 + 0.05) {  // 按下按键2
            temp_btn1_state = false;
            temp_btn2_state = true;
        } else if (voltage > V_11 - 0.05 && voltage < V_11 + 0.05) {  // 同时按下
            temp_btn1_state = true;
            temp_btn2_state = true;
        }

        // 检查状态是否发生变化
        if (temp_btn1_state != current_btn1_state || temp_btn2_state != current_btn2_state) {
            // 状态发生变化，重置计时器
            state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            current_btn1_state = temp_btn1_state;
            current_btn2_state = temp_btn2_state;
        } else {
            // 状态未变化，检查持续时间
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (current_time - state_start_time >= STATE_DURATION_MS) {
                // 持续时间达到阈值，触发状态变化
                if (current_btn1_state != last_btn1_state) {
                    pair->btn1->isPressed = current_btn1_state;
                    pair->btn1->isrTriggered = true;
                    last_btn1_state = current_btn1_state;
                    // ESP_LOGI(TAG, "gpio: %d, voltage: %f", pair->gpio, voltage);
                }
                if (current_btn2_state != last_btn2_state) {
                    pair->btn2->isPressed = current_btn2_state;
                    pair->btn2->isrTriggered = true;
                    last_btn2_state = current_btn2_state;
                    // ESP_LOGI(TAG, "gpio: %d, voltage: %f", pair->gpio, voltage);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    delete pair;
    vTaskDelete(NULL);
}

void button_adc2_task(void* arg) {
    button_adc2_pair_t* pair = (button_adc2_pair_t*)arg;

    // 记录上一次的状态
    bool last_btn1_state = false;
    bool last_btn2_state = false;

    // 当前状态
    bool current_btn1_state = false;
    bool current_btn2_state = false;

    // 用于记录状态持续时间
    uint32_t state_start_time = 0;
    const uint32_t STATE_DURATION_MS = 50;  // 状态持续时间阈值

    while (pair->running) {
        // 单次采样
        int adc_value; 
        adc2_get_raw(pair->adc2_chan, ADC_WIDTH_BIT_12, &adc_value);
        float voltage = adc_value * 3.3 / 4095;

        // 临时状态变量
        bool temp_btn1_state = false;
        bool temp_btn2_state = false;

        // 判断当前状态
        if (voltage > V_00 - 0.02 && voltage < V_00 + 0.02) {  // 都未按下
            temp_btn1_state = false;
            temp_btn2_state = false;
        } else if (voltage > V_01 - 0.02 && voltage < V_01 + 0.02) {  // 按下按键1
            temp_btn1_state = true;
            temp_btn2_state = false;
        } else if (voltage > V_10 - 0.1 && voltage < V_10 + 0.05) {  // 按下按键2
            temp_btn1_state = false;
            temp_btn2_state = true;
        } else if (voltage > V_11 - 0.05 && voltage < V_11 + 0.05) {  // 同时按下
            temp_btn1_state = true;
            temp_btn2_state = true;
        }

        // 检查状态是否发生变化
        if (temp_btn1_state != current_btn1_state || temp_btn2_state != current_btn2_state) {
            // 状态发生变化，重置计时器
            state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            current_btn1_state = temp_btn1_state;
            current_btn2_state = temp_btn2_state;
        } else {
            // 状态未变化，检查持续时间
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (current_time - state_start_time >= STATE_DURATION_MS) {
                // 持续时间达到阈值，触发状态变化
                if (current_btn1_state != last_btn1_state) {
                    pair->btn1->isPressed = current_btn1_state;
                    pair->btn1->isrTriggered = true;
                    last_btn1_state = current_btn1_state;
                    // ESP_LOGI(TAG, "gpio: %d, voltage: %f", pair->gpio, voltage);
                }
                if (current_btn2_state != last_btn2_state) {
                    pair->btn2->isPressed = current_btn2_state;
                    pair->btn2->isrTriggered = true;
                    last_btn2_state = current_btn2_state;
                    // ESP_LOGI(TAG, "gpio: %d, voltage: %f", pair->gpio, voltage);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    delete pair;
    vTaskDelete(NULL);
}

// 重载函数 - ADC1 版本
void Button::register_share_io_2(gpio_num_t gpio, adc1_channel_t adc1_chan, Button* button1, Button* button2)
{    
    button_adc1_pair_t* pair = new button_adc1_pair_t{
        .btn1 = button1,
        .btn2 = button2,
        .gpio = gpio,
        .running = true,
        .adc1_chan = adc1_chan
    };

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "button_adc_gpio%d", gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // ADC1 配置
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc1_chan, ADC_ATTEN_DB_12);

    xTaskCreate(button_adc1_task, task_name, 2048, pair, 5, NULL);
}

// 重载函数 - ADC2 版本
void Button::register_share_io_2(gpio_num_t gpio, adc2_channel_t adc2_chan, Button* button1, Button* button2)
{    
    button_adc2_pair_t* pair = new button_adc2_pair_t{
        .btn1 = button1,
        .btn2 = button2,
        .gpio = gpio,
        .running = true,
        .adc2_chan = adc2_chan
    };

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "button_adc_gpio%d", gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // ADC2 配置
    adc2_config_channel_atten(adc2_chan, ADC_ATTEN_DB_12);

    xTaskCreate(button_adc2_task, task_name, 2048, pair, 5, NULL);
}
//----------------------------------------------单个IO控制两个按钮----------------------------------------------//
