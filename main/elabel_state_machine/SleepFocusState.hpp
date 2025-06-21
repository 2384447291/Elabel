#ifndef  SLEEPFOCUSSTATE_HPP
#define  SLEEPFOCUSSTATE_HPP

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
#include "FocusTaskState.hpp"

//等待收同步消息的间隔
#define ESPNOW_WAITING_TIME 100

//当时间>300，每5min响一次
//当时间>60,<300，每1min响一次
//当时间<60，每10s响一次
//当时间小于0，每10s响一次

class SleepFocusState : public State<ElabelController>
{
public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);
    //是否需要跳出该状态
    bool need_out_state = false;
    //下一次唤醒的时间
    int16_t next_wake_up_time = 0;

    static SleepFocusState* Instance()
    {
        static SleepFocusState instance;
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

    int get_inner_countdown_time()
    {
        // 计算剩余秒数，可能为负（超时）
        int inner_time_s = FocusTaskState::Instance()->choose_task_fall_timing
                        - int((get_unix_time() - FocusTaskState::Instance()->choose_task_start_time) / 1000);

        // 四舍五入到分钟
        int abs_seconds = abs(inner_time_s);
        int minutes = abs_seconds / 60;
        int seconds = abs_seconds % 60;

        if (seconds >= 30) {
            minutes++;
        }

        int rounded_seconds = minutes * 60;

        // 恢复原始符号：倒计时为正，超时为负
        if (inner_time_s < 0) {
            return -rounded_seconds;
        } else {
            return rounded_seconds;
        }
    }

    void calculate_wake_up_time()
    {
        uint32_t focus_time =  FocusTaskState::Instance()->choose_task_fall_timing - (get_unix_time() - FocusTaskState::Instance()->choose_task_start_time)/1000;
        //如果倒计时时间大于5分钟
        if(focus_time > 300)
        {
            next_wake_up_time = focus_time % 300;
        }
        else if(focus_time > 60)
        {
            next_wake_up_time = focus_time % 60;
        }
        else if(focus_time > 0)
        {
            next_wake_up_time = focus_time % 10;
        }
        else
        {
            next_wake_up_time = 9;
        }
        //加一使得下次取余顺利
        next_wake_up_time++;
    }

    void enter_sleep()
    {
        calculate_wake_up_time();
        printf("Enter Sleep, next_wake_up_time: %d\n", next_wake_up_time);

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
        if(EspNowSlave::Instance()->sleep_sync_flag == 2)
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
                //如果是focus同步则说明还在focus状态
                if(EspNowSlave::Instance()->sleep_sync_flag == 1)
                {
                    break;
                }
                //如果在同步模式则说明退出了focus状态
                else if(EspNowSlave::Instance()->sleep_sync_flag == 2)
                {
                    need_out_state = true;
                    break;
                }
            }
        }   

        if(!need_out_state)
        {
            BatteryManager::Instance()->setPowerState(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            //播放音乐
            FocusTaskState::Instance()->play_focus_music();
            lock_lvgl();
            switch_screen(ui_FocusScreen);
            FocusTaskState::Instance()->inner_time_countdown_s = get_inner_countdown_time();
            FocusTaskState::Instance()->flush_focus_time();
            release_lvgl();
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
};

#endif