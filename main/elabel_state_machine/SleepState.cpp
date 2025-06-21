#include "SleepState.hpp"
#include "control_driver.hpp"
#include "global_time.h"
#include "global_draw.h"
#include "control_driver.hpp"
#include "global_draw.h"

void SleepState::Init(ElabelController* pOwner)
{
}

void SleepState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter SleepState.");
    next_wake_up_time = 0;
    need_out_state = false;
    start_sleep_time = 0;
    need_clock_mode = get_global_data()->m_device_info.is_idel_clock_time;
    //同步时间戳
    EspNowSlave::Instance()->slave_send_espnow_http_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));

    //如果需要显示idel_clock,则切换界面
    if(need_clock_mode)
    {
        char clock_time[6];
        get_clock_time(clock_time);
        memcpy(show_clock_time, clock_time, 6);
        lock_lvgl();
        switch_screen(ui_SleepScreen);
        set_text_without_change_font(ui_SleepCLock, clock_time);
        release_lvgl();
    }
    //把所有choosetask的内容改为静止的
    else
    {
        lock_lvgl();
        uint8_t child_count = lv_obj_get_child_cnt(ui_TaskContainer);
        for(int i = 0; i<child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(ui_TaskContainer, i);
            lv_obj_t *ui_Label1 = lv_obj_get_child(child, 0);
            lv_label_set_long_mode(ui_Label1, LV_LABEL_LONG_DOT);
            for(int j = 0; j < lv_obj_get_child_cnt(ui_Label1); j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(ui_Label1, j);
                lv_label_set_long_mode(label_child, LV_LABEL_LONG_DOT);
            }
        }
        release_lvgl();
    }

    //关闭其他额外线程
    ControlDriver::Instance()->stop_button_check_task();
    if(!need_clock_mode)
    {
        suspend_gui();
    }

    EspNowSlave::Instance()->slave_send_espnow_http_sleep_request();
    //等待2s页面刷新
    vTaskDelay(pdMS_TO_TICKS(2000));
    EspNowSlave::Instance()->sleep_sync_flag = 0;
    start_sleep();
}
void SleepState::Execute(ElabelController* pOwner)
{
    
}

void SleepState::Exit(ElabelController* pOwner)
{
    BatteryManager::Instance()->setPowerState(true);
    ESP_ERROR_CHECK(esp_wifi_start());
    EspNowSlave::Instance()->resume_espnow();

    ControlDriver::Instance()->start_button_check_task();
    if(!need_clock_mode)
    {
        resume_gui();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}

