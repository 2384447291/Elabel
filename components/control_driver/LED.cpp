#include "LED.hpp"

LED::LED()
{
    led_state = LedState::NO_LIGHT;
    previous_state = LedState::NO_LIGHT;
}

void LED::setLedState(LedState state)
{
    led_state = state;
}

LedState LED::getLedState()
{
    return led_state;
}

void LED::init()
{
    // LED初始化
    led_strip = ws2812_create();
}

void LED::handle()
{
    // 只在状态发生改变时更新LED
    if (previous_state != led_state)
    {
        switch(led_state)
        {
            case LedState::DEVICE_INIT:
                led_set_on(led_strip, (led_color_t){0, 50, 0});
                break;
            case LedState::DEVICE_NO_WIFI:
                led_set_on(led_strip, (led_color_t){50, 0, 0});
                break;
            case LedState::DEVICE_WIFI_CONNECT:
                led_set_on(led_strip, (led_color_t){0, 0, 50});
                break;
            case LedState::NO_LIGHT:
                led_set_off(led_strip);
                break;
        }
        previous_state = led_state;  // 更新状态
    }
}   
