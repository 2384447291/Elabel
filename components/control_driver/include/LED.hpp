#ifndef LED_HPP
#define LED_HPP
#include "ws2812_control.h"
enum class LedState {
    NO_LIGHT = 0,
    DEVICE_INIT,
    DEVICE_NO_WIFI,
    DEVICE_WIFI_CONNECT,
};
class LED
{
public:
    void handle();  // 处理定时任务
    LED();
    void init();
    void setLedState(LedState state);
    LedState getLedState();
private:
    ws2812_strip_t* led_strip;
    LedState led_state;
    LedState previous_state;  // 用于跟踪上一次的状态
};
#endif