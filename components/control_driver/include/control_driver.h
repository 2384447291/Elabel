#ifndef CONTROL_DRIVER_H
#define CONTROL_DRIVER_H
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "global_message.h"
#include "ws2812_control.h"

#define CALL_BACK_MAX 10

// 按钮结构体
typedef struct {
    gpio_num_t gpio_num;                // GPIO编号
    gpio_num_t led_gpio;                // 关联的LED GPIO编号
    uint32_t debounce_delay;            // 防抖延迟
    uint32_t long_press_time;           // 长按时间阈值
    volatile uint32_t last_interrupt_time; // 上次中断时间
    uint32_t press_start_time;          // 按下时刻（任务中记录）      // 按钮状态
    bool button_pressed;              
    bool button_released;               
    const char *name;                   // 按钮名称
} button_t;

typedef enum {
    no_light = 0,
    device_init,
    device_no_wifi,
    device_wifi_connect,
    device_focus,
    device_out_time,
} led_state;

#ifdef __cplusplus
extern "C" {
#endif
void control_gpio_init(void);

void buzzer_pwm_start(uint32_t new_frequency);
void buzzer_pwm_stop();
void update_buzzer_state(uint32_t Time_count_down ,float _duration, float _count, float _delay, float _buzzer_time);

int get_EncoderValue();
void reset_EncoderValue();
void set_EncoderValue(int value);

void set_led_state(led_state _led_state);
void set_todo_time_up(uint32_t _time,uint32_t max_time);

void register_button_down_short_press_call_back(void (*call_back)());
void register_button_down_long_press_call_back(void (*call_back)());
void register_button_up_short_press_call_back(void (*call_back)());
void register_button_up_long_press_call_back(void (*call_back)());
void register_encoder_right_circle_call_back(void (*call_back)());
void register_encoder_left_circle_call_back(void (*call_back)());

void unregister_button_down_short_press_call_back(void (*call_back)());
void unregister_button_down_long_press_call_back(void (*call_back)());
void unregister_button_up_short_press_call_back(void (*call_back)());
void unregister_button_up_long_press_call_back(void (*call_back)());
void unregister_encoder_right_circle_call_back(void (*call_back)());
void unregister_encoder_left_circle_call_back(void (*call_back)());
#ifdef __cplusplus
}
#endif


#endif