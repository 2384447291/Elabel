#ifndef CONTROL_DRIVER_H
#define CONTROL_DRIVER_H
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "global_message.h"

typedef enum{
    short_press,
    long_press,
} button_interrupt;

typedef enum{
    right_circle,
    left_circle,
} encoder_interrupt;

#ifdef __cplusplus
extern "C" {
#endif
void control_gpio_init(void);
button_interrupt get_button_interrupt(void);
encoder_interrupt get_encoder_interrupt(void); 
#ifdef __cplusplus
}
#endif


#endif