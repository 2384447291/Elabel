#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "driver/gpio.h"
#include "esp_log.h"
#include "callback.hpp"
#include "driver/adc.h"

//----------------------------------------------Button类定义----------------------------------------------//
class Button {
public:
    enum class State {
        IDLE,               // 空闲状态
        PRESSED,            // 按下状态
        WAIT_RELEASE,       // 等待释放状态
        WAIT_DOUBLE_CLICK   // 等待双击状态
    };

    Button(const char* buttonName, uint32_t longPressTime, uint32_t doubleClickTime);
    void handle();
    Callback CallbackShortPress{"SHORT Press"};
    Callback CallbackLongPress{"LONG Press"};
    Callback CallbackDoubleClick{"DOUBLE Click"};

    const char* name;
    uint32_t longPressTime;
    uint32_t doubleClickTime;
    
    // 状态相关
    State state;
    uint32_t pressTime;
    uint32_t lastPressTime;
    bool isPressed;
    volatile bool isrTriggered;

    //不同的按钮数据来源
    gpio_num_t Single_IO; //单个IO控制一个button
    static void register_single_io(gpio_num_t gpio, Button* button);
    gpio_num_t Share_IO_2; //单个IO控制两个button
    static void register_share_io_2(gpio_num_t gpio, adc1_channel_t adc_chan, Button* button1, Button* button2);
    static void register_share_io_2(gpio_num_t gpio, adc2_channel_t adc_chan, Button* button1, Button* button2);
};
//----------------------------------------------Button类定义----------------------------------------------//




#endif

