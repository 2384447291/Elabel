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

//等待收同步消息的间隔
#define ESPNOW_WAITING_TIME 100
#define SLEEP_FOCUS_COUTDOWN 15*1000

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
    bool need_out_focus = false;

    //当前focus的类型1是纯时间，2是task，3是record
    uint8_t focus_type = 0;
    //当前focus的record_message_unique_id,从任务名字中获取，需要和mcodec里的对应
    uint32_t focus_record_message_unique_id = 0;

    //任务描述
    int choose_task_fall_timing = 0;
    long long choose_task_start_time = 0;
    char choose_task_title[100] = "";

    //是否进入了sleep
    bool is_sleep_focus = false;
    //进入focus的倒计时
    uint32_t sleep_count = SLEEP_FOCUS_COUTDOWN;

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

    void play_focus_music()
    {
        if(focus_type == 3)
        {
            if(focus_record_message_unique_id == MCodec::Instance()->record_message_unique_id)
            {
                MCodec::Instance()->play_mic();
            }
            else
            {
                if(get_global_data()->m_device_info.is_strong_wake_up)
                {
                    MCodec::Instance()->play_music("bell");
                }
                else
                {
                    MCodec::Instance()->play_music("tick");
                }
            }
        }
        else
        {
            if(get_global_data()->m_device_info.is_strong_wake_up)
            {
                MCodec::Instance()->play_music("bell");
            }
            else
            {
                MCodec::Instance()->play_music("tick");
            }
        }
    }

    int get_inner_countdown_time()
    {
        // 计算剩余秒数，可能为负（超时）
        int inner_time_s = choose_task_fall_timing - int((get_unix_time() - choose_task_start_time) / 1000);

        int abs_seconds = abs(inner_time_s);
        int rounded_seconds;

        if (inner_time_s < 0) {
            // 负数：不管多大，统一按 10 秒为格四舍五入
            int tens = (abs_seconds + 5) / 10;  // +5 用于四舍五入
            rounded_seconds = tens * 10;
            return -rounded_seconds;
        }

        // 正数：不足 1 分钟按 10 秒格，1 分钟及以上按分钟四舍五入
        if (abs_seconds < 60) {
            int tens = (abs_seconds + 5) / 10;
            rounded_seconds = tens * 10;
        } else {
        // 1 分钟及以上：按分钟四舍五入（秒 ≥30s 向上进 1 分钟）
            int minutes = abs_seconds / 60;
            int seconds = abs_seconds % 60;
            if (seconds >= 30) {
                minutes++;
            }
            rounded_seconds = minutes * 60;
        }

        return rounded_seconds;
    }



    void calculate_wake_up_time()
    {
        int focus_time =  choose_task_fall_timing - (get_unix_time() - choose_task_start_time)/1000;
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
            if(get_global_data()->m_device_info.overtime_alert_time == 0) return;
            int abs_seconds = -focus_time;
            next_wake_up_time = 10 - abs_seconds % (get_global_data()->m_device_info.overtime_alert_time);
        }
        //加一使得下次取余顺利
        next_wake_up_time++;
    }

    void set_time_str_and_process(bool is_sleep)
    {
        //设置倒计时
        int inner_time_countdown_s = 0;
        char timestr[20];
        if(is_sleep)
        {
            inner_time_countdown_s = get_inner_countdown_time();
            if(inner_time_countdown_s >= 0)
            {
                sprintf(timestr, "< %02d:%02d", inner_time_countdown_s / 60, inner_time_countdown_s % 60);
            }
            else
            {
                sprintf(timestr, "+ %02d:%02d", (-inner_time_countdown_s) / 60, (-inner_time_countdown_s) % 60);
            }
        }
        else
        {
            inner_time_countdown_s = choose_task_fall_timing - int((get_unix_time() - choose_task_start_time) / 1000);
            if(inner_time_countdown_s >= 0)
            {
                sprintf(timestr, "%02d:%02d", inner_time_countdown_s / 60, inner_time_countdown_s % 60);
            }
            else
            {
                sprintf(timestr, "+ %02d:%02d", (-inner_time_countdown_s) / 60, (-inner_time_countdown_s) % 60);
            }
        }

        set_text_without_change_font(ui_SleepTaskFocusTime, timestr);
        set_text_without_change_font(ui_SleepTimeFocusTime, timestr);  

        //设置进度条
        int process_length = 0;
        if(inner_time_countdown_s >= 0)
        {
            process_length =  round(360.0f *( 1 - (float)(inner_time_countdown_s)/(float)(choose_task_fall_timing)));
            lv_arc_set_value(ui_MinuteBar, process_length);
        }
        else
        {
            process_length =  360.0f;
        }
        lv_arc_set_value(ui_CoutdownBar, process_length);
    }


    void enter_sleep()
    {
        calculate_wake_up_time();
        int focus_time =  choose_task_fall_timing - (get_unix_time() - choose_task_start_time)/1000;
        printf("Enter Sleep, next_wake_up_time: %d, focus_time: %d\n", next_wake_up_time, focus_time);

        //关闭wifi
        ESP_ERROR_CHECK(esp_wifi_stop());
        //关闭外设电源
        BatteryManager::Instance()->setPowerState(false);
        ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));

        //不设置唤醒源，light-sleep-enter没有用
        uint64_t mask = (1ULL << DEVICE_BUTTON_1234) | (1ULL << DEVICE_BUTTON_5678);
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
           //唤醒lvgl和硬件开关
            BatteryManager::Instance()->setPowerState(true);
            resume_gui();
            vTaskDelay(pdMS_TO_TICKS(500));
            
            //播放音乐
            play_focus_music();

            lock_lvgl();
            switch_screen(ui_SleepFocusScreen);
            set_time_str_and_process(is_sleep_focus);
            release_lvgl();

            //等待墨水瓶响应和关闭ui线程
            vTaskDelay(pdMS_TO_TICKS(WAITING_BEFORE_SLEEP_TIME));
            suspend_gui();
        }
    }
};

#endif