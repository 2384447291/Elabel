#include "button.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "callback.hpp"
#include "global_message.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "control_driver.hpp"

#define TAG "BUTTON"
#define R_Bais 20.0
#define R0 10.0
#define R1 3.0
#define R2 0.0
#define R3 30.0

#define V_BUTTON_0 (3.3*(R_Bais/(R_Bais+R0)))
#define V_BUTTON_1 (3.3*(R_Bais/(R_Bais+R1)))  
#define V_BUTTON_2 (3.3*(R_Bais/(R_Bais+R2)))  
#define V_BUTTON_3 (3.3*(R_Bais/(R_Bais+R3))) 
 
#define ERROR_RANGE 0.2


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
    int raw = 0;
    int adc_value = 0;

    ControlDriver::Instance()->lock_adc();
    esp_err_t r = adc_oneshot_read(adc_handle, adc_channel, &raw);
    ControlDriver::Instance()->release_adc();

    if (r != ESP_OK) 
    {
        ESP_LOGE(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(r));
    } 
    else 
    {
        if (cali_handle) 
        {
            esp_err_t r2 = adc_cali_raw_to_voltage(cali_handle, raw, &adc_value);
            if (r2 != ESP_OK) 
            {
                ESP_LOGE(TAG, "adc_cali_raw_to_voltage error: %s", esp_err_to_name(r2));
            }
        } 
        else 
        {
            ESP_LOGE(TAG, "Raw ADC (no calibration): %d", raw);
        }
    }

    float voltage = (float)adc_value/ 1000.0f; // 转换为V

    // 临时状态变量,默认都是0
    bool temp_button_state[3] = {false, false, false};
    // 判断当前状态
    if (voltage > V_BUTTON_0 - ERROR_RANGE && voltage < V_BUTTON_0 + ERROR_RANGE) {  // 都未按下
        // ESP_LOGI(TAG, "voltage: %f, adc_value: %d", voltage, adc_value);
        temp_button_state[0] = true;
        temp_button_state[1] = false;
        temp_button_state[2] = false;
    } else if (voltage > V_BUTTON_1 - ERROR_RANGE && voltage < V_BUTTON_1 + ERROR_RANGE) {  // 按下按键1
        // ESP_LOGI(TAG, "voltage: %f, adc_value: %d", voltage, adc_value);
        temp_button_state[0] = false;
        temp_button_state[1] = true;
        temp_button_state[2] = false;
    } else if (voltage > V_BUTTON_2 - ERROR_RANGE && voltage < V_BUTTON_2 + ERROR_RANGE) {  // 按下按键2
        // ESP_LOGI(TAG, "voltage: %f, adc_value: %d", voltage, adc_value);
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

void Button_pair_3::init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button0, Button* _button1, Button* _button2)
{    
    button[0] = _button0;
    button[1] = _button1;
    button[2] = _button2;
    adc_handle = _adc_handle;
    adc_channel = _adc_channel;

    // 创建校准方案句柄
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_cali_create_scheme_curve_fitting failed: %s", esp_err_to_name(ret));
        // 若返回 ESP_ERR_NOT_SUPPORTED，可继续使用原始读数
    } else {
        ESP_LOGI(TAG, "Calibration scheme created");
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &chan_cfg));  
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
    int raw = 0;
    int adc_value = 0;

    ControlDriver::Instance()->lock_adc();
    esp_err_t r = adc_oneshot_read(adc_handle, adc_channel, &raw);
    ControlDriver::Instance()->release_adc();

    if (r != ESP_OK) 
    {
        ESP_LOGE(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(r));
    } 
    else 
    {
        if (cali_handle) 
        {
            esp_err_t r2 = adc_cali_raw_to_voltage(cali_handle, raw, &adc_value);
            if (r2 != ESP_OK) 
            {
                ESP_LOGE(TAG, "adc_cali_raw_to_voltage error: %s", esp_err_to_name(r2));
            }
        } 
        else 
        {
            ESP_LOGE(TAG, "Raw ADC (no calibration): %d", raw);
        }
    }

    float voltage = (float)adc_value/ 1000.0f; // 转换为V

    bool temp_button_state = false;

    // 判断当前状态
    if(voltage > 1.5f)
    {
        temp_button_state = true;
    }
    
    // 检查状态是否发生变化
    if (temp_button_state != current_button_state) 
    {
        // 状态发生变化，重置计时器
        state_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        current_button_state = temp_button_state;
    } 
    else 
    {
        // 状态未变化，检查持续时间
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - state_start_time >= STATE_DURATION_MS) 
        {
            // 持续时间达到阈值，触发状态变化
            if (current_button_state != last_button_state) 
            {
                button->isPressed = current_button_state;
                button->isrTriggered = true;
                last_button_state = current_button_state;
            }
        }
    }
}

void Button_pair_1::init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button)
{    
    button = _button;
    adc_handle = _adc_handle;
    adc_channel = _adc_channel;

    // 创建校准方案句柄
    adc_cali_curve_fitting_config_t cali_config = { 
        .unit_id = ADC_UNIT_1,
        .chan = adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_cali_create_scheme_curve_fitting failed: %s", esp_err_to_name(ret));
        // 若返回 ESP_ERR_NOT_SUPPORTED，可继续使用原始读数
    } else {
        ESP_LOGI(TAG, "Calibration scheme created");
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &chan_cfg));  
}

void Button_pair_1::clear_state()
{
    current_button_state = false;
    last_button_state = false;
    button->clear_state();
}



//----------------------------------------------单个IO控制一个按钮----------------------------------------------//


//----------------------------------------------单个IO控制四个按钮----------------------------------------------//
void Button_pair_4::update() 
{
    int raw = 0;
    int adc_value = 0;

    ControlDriver::Instance()->lock_adc();
    esp_err_t r = adc_oneshot_read(adc_handle, adc_channel, &raw);
    ControlDriver::Instance()->release_adc();

    if (r != ESP_OK) 
    {
        ESP_LOGE(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(r));
    } 
    else 
    {
        if (cali_handle) 
        {
            esp_err_t r2 = adc_cali_raw_to_voltage(cali_handle, raw, &adc_value);
            if (r2 != ESP_OK) 
            {
                ESP_LOGE(TAG, "adc_cali_raw_to_voltage error: %s", esp_err_to_name(r2));
            }
        } 
        else 
        {
            ESP_LOGE(TAG, "Raw ADC (no calibration): %d", raw);
        }
    }

    float voltage = (float)adc_value/ 1000.0f; // 转换为V

    // 临时状态变量,默认都是0
    bool temp_button_state[4] = {false, false, false, false};
    // 判断当前状态
    if (voltage > V_BUTTON_0 - ERROR_RANGE && voltage < V_BUTTON_0 + ERROR_RANGE) {  // 按下按键0
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

void Button_pair_4::init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button0, Button* _button1, Button* _button2, Button* _button3)
{    
    button[0] = _button0;
    button[1] = _button1;
    button[2] = _button2;
    button[3] = _button3;
    adc_handle = _adc_handle;
    adc_channel = _adc_channel;

    // 创建校准方案句柄
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_cali_create_scheme_curve_fitting failed: %s", esp_err_to_name(ret));
        // 若返回 ESP_ERR_NOT_SUPPORTED，可继续使用原始读数
    } else {
        ESP_LOGI(TAG, "Calibration scheme created");
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &chan_cfg));  
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