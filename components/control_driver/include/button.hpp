#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "driver/gpio.h"
#include "esp_log.h"
#include "callback.hpp"
#include "esp_adc_cal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

//----------------------------------------------Button类定义----------------------------------------------//
class Button {
public:
    enum class State {
        IDLE,           // 空闲状态
        PRESSED,        // 按下状态
        WAITING_RELEASE, // 等待释放状态
    };

    Button(const char* buttonName, uint32_t longPressTime);
    void handle();
    void clear_state(){
        isPressed = false;
        isrTriggered = false;
        state = State::IDLE;
        pressTime = 0;
        lastPressTime = 0;
    }
    Callback CallbackShortPress{"SHORT Press"};
    Callback CallbackLongPress{"LONG Press"};

    const char* name;
    uint32_t longPressTime;
    
    // 状态相关
    State state;
    uint32_t pressTime;
    uint32_t lastPressTime;
    bool isPressed;
    volatile bool isrTriggered;
};
//----------------------------------------------Button类定义----------------------------------------------//


//----------------------------------------------Button_pair_1类定义----------------------------------------------//
class Button_pair_1 {
public:
    Button_pair_1(){};

    //adc读取参数
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t cali_handle = NULL;
    adc_channel_t adc_channel;

    Button* button;
    void update();
    void init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button);
    void clear_state();
    bool current_button_state = false;
    bool last_button_state = false;
    uint32_t state_start_time = 0;
};
//----------------------------------------------Button_pair_1类定义----------------------------------------------//


//-------------------------------------------Button_pair_3类定义-------------------------------------------//
class Button_pair_3 {
public:
    Button_pair_3(){};
    Button* button[3];

    //adc读取参数
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t cali_handle = NULL;
    adc_channel_t adc_channel;

    // 当前状态
    bool current_button_state[3] = {false, false, false};
    // 上一次状态
    bool last_button_state[3] = {false, false, false};
    // 状态持续时间
    uint32_t state_start_time = 0;

    void init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button0, Button* _button1, Button* _button2);
    void update();
    void clear_state();
};
//-------------------------------------------Button_pair_3类定义-------------------------------------------//


//-------------------------------------------Button_pair_4类定义-------------------------------------------//
class Button_pair_4 {
public:
    Button_pair_4(){};
    Button* button[4];

    //adc读取参数
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t cali_handle = NULL;
    adc_channel_t adc_channel;

    // 当前状态
    bool current_button_state[4] = {false, false, false, false};
    // 上一次状态
    bool last_button_state[4] = {false, false, false, false};
    // 状态持续时间
    uint32_t state_start_time = 0;

    void init(adc_oneshot_unit_handle_t _adc_handle, adc_channel_t _adc_channel, Button* _button0, Button* _button1, Button* _button2, Button* _button3);
    void update();
    void clear_state();
};
//-------------------------------------------Button_pair_4类定义-------------------------------------------//

#endif

