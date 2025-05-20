#ifndef  SLEEPSTATE_HPP
#define  SLEEPSTATE_HPP

#include "StateMachine.hpp"
#include <cmath>
#include "ElabelController.hpp"
#include "battery_manager.hpp"
#include "network.h"
#include "control_driver.hpp"
#include "esp_timer.h"
#include "driver/uart.h"
#include "esp_now_slave.hpp"

#define WAKEUP_INTERVAL_SEC 10

class SleepState : public State<ElabelController>
{
public:

    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);
    bool need_out_state = false;
    char show_clock_time[6];
    int64_t start_sleep_time = 0;
    
    static SleepState* Instance()
    {
        static SleepState instance;
        return &instance;
    }

    void enter_sleep()
    {
        printf("Enter Sleep\n");
        //关闭wifi
        ESP_ERROR_CHECK(esp_wifi_stop());
        //关闭外设电源
        BatteryManager::Instance()->setPowerState(false);
        ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
        //不设置唤醒源，light-sleep-enter没有用
        gpio_wakeup_enable(DEVICE_BUTTON_1234, GPIO_INTR_HIGH_LEVEL);
        gpio_wakeup_enable(DEVICE_BUTTON_567, GPIO_INTR_HIGH_LEVEL);
        gpio_wakeup_enable(DEVICE_BUTTON_8, GPIO_INTR_HIGH_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(WAKEUP_INTERVAL_SEC * 1000000ULL));
        //进入休眠
        int64_t t_before_us = esp_timer_get_time();
        uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM);
        esp_light_sleep_start();
        //唤醒后的判断
        int64_t t_after_us = esp_timer_get_time();
        int64_t slept_ms = (t_after_us - t_before_us) / 1000;
        
        if(slept_ms < WAKEUP_INTERVAL_SEC*1000)
        {
            printf("Woke up from sleep GPIO interrupt\n");
        }
        else
        {
            printf("Woke up from sleep timer\n");
        }
    }


    void get_message()
    {   
        EspNowSlave::Instance()->sleep_sync_flag = false;
        esp_err_t ret = EspNowSlave::Instance()->slave_send_espnow_http_synchronous_request();

        if(ret == ESP_OK)
        {
            
        }
        else
        {
            
        }

        char clock_time[6];
        get_clock_time(clock_time);
        if(strcmp(clock_time, show_clock_time) != 0)
        {
            lock_lvgl();
            set_text_without_change_font(ui_SleepCLock, clock_time);
            memcpy(show_clock_time, clock_time, 6);
            release_lvgl();
        }
        enter_sleep();
    }

    void out_sleep()
    {
        need_out_state = true;
    }
};

#endif