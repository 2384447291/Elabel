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
#include "esp_now_client.hpp"
#include "esp_now_slave.hpp"
#include <vector>

//等待收同步消息的间隔
#define ESPNOW_WAITING_TIME 100
#define IDLE_CLOCK_WAKE_UP_TIME 60

class SleepState : public State<ElabelController>
{
public:

    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);
    //是否需要跳出该状态
    bool need_out_state = false;
    //是否需要显示clock时间
    bool need_clock_mode = false;
    //开始休眠的时间
    int64_t start_sleep_time = 0;
    //下一次唤醒的时间
    uint16_t next_wake_up_time = 0;
    //显示的clock时间
    char show_clock_time[6];

    static SleepState* Instance()
    {
        static SleepState instance;
        return &instance;
    }

    void start_sleep()
    {
        need_out_state = false;
        while(!need_out_state)
        {
            enter_sleep();
        }
    }

    void calculate_wake_up_time()
    {
        if(need_clock_mode)
        {
            next_wake_up_time = IDLE_CLOCK_WAKE_UP_TIME;
        }
        else
        {
            next_wake_up_time = get_global_data()->m_device_info.sleep_time;
        }
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
        #ifdef R01A_TEST
        uint64_t mask = (1ULL << DEVICE_BUTTON_1234) | (1ULL << DEVICE_BUTTON_567) | (1ULL << DEVICE_BUTTON_8);
        #elif defined(R01B_TEST)
        uint64_t mask = (1ULL << DEVICE_BUTTON_1234) | (1ULL << DEVICE_BUTTON_5678);
        #endif
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH));  // 任意引脚高电平触发唤醒
        calculate_wake_up_time();
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(next_wake_up_time * 1000000ULL));
        //进入休眠
        int64_t t_before_us = esp_timer_get_time();
        esp_light_sleep_start();
        //唤醒后的判断
        int64_t t_after_us = esp_timer_get_time();
        int64_t slept_ms = (t_after_us - t_before_us) / 1000;
        //给0.5s的容差
        if(slept_ms < (next_wake_up_time - 0.5)*1000)
        {
            printf("Woke up from sleep GPIO interrupt, time: %lld\n", slept_ms);
            need_out_state = true;
        }
        else
        {
            printf("Woke up from sleep timer, time: %lld\n", slept_ms);
            get_message();
        }
    }


    void get_message()
    {   
        ESP_ERROR_CHECK(esp_wifi_start());
        EspNowSlave::Instance()->resume_espnow();
        //防止数据丢失
        if(EspNowSlave::Instance()->sleep_sync_flag == 1)
        {
            need_out_state = true;
            EspNowSlave::Instance()->sleep_sync_flag = 0;
            return;
        }
        EspNowSlave::Instance()->sleep_sync_flag = 0;
        esp_err_t ret = EspNowSlave::Instance()->slave_send_espnow_http_synchronous_request();

        if(ret == ESP_OK)
        {
            int64_t start_time = esp_timer_get_time();
            //在这段时间内不断检查这个标记
            while((esp_timer_get_time() - start_time) < ESPNOW_WAITING_TIME * 1000ULL)
            {
                if(EspNowSlave::Instance()->sleep_sync_flag == 1)
                {
                    need_out_state = true;
                    break;
                }
                else if(EspNowSlave::Instance()->sleep_sync_flag == 2)
                {
                    break;
                }
            }
        }   
        
        //更新时间
        if(need_clock_mode)
        {
            char clock_time[6];
            get_clock_time(clock_time);
            if(strcmp(clock_time, show_clock_time) != 0)
            {
                memcpy(show_clock_time, clock_time, 6);
                BatteryManager::Instance()->setPowerState(true);
                vTaskDelay(pdMS_TO_TICKS(500));
                lock_lvgl();
                set_text_without_change_font(ui_SleepCLock, clock_time);
                release_lvgl();
                vTaskDelay(pdMS_TO_TICKS(2500));
            }
        }
    }
};

#endif