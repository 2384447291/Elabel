#include "FocusTaskState.hpp"
#include "control_driver.hpp"
#include "network.h"
#include "http.h"
#include "httpmusic.h"
#include "ssd1680.h"
#include "codec.hpp"
#include "Esp_now_slave.hpp"

void outfocus()
{
    if(FocusTaskState::Instance()->need_out_focus) return;
    play_finish_task_sound();
    FocusTaskState::Instance()->need_out_focus = true;
    if(get_global_data()->m_is_host == 1)
    {
        char sstr[12];
        sprintf(sstr, "%d", get_global_data()->focusing_task_id);
        http_out_focus(sstr,false);
    }
    else if(get_global_data()->m_is_host == 2)
    {
        focus_message_t focus_message = pack_focus_message(0, 0, 0,get_global_data()->focusing_task_id, (char*)"");
        EspNowSlave::Instance()->slave_send_espnow_http_out_focus_task(focus_message);
    }
}

void FocusTaskState::Init(ElabelController* pOwner)
{
    
}

void FocusTaskState::Enter(ElabelController* pOwner)
{
    // 进入focus的时候，重置卡死时间,这个代表的是outfocus的倒计时
    pOwner->stuck_time = 0;
    last_beep_time = INT32_MIN;
    inner_time_countup_ms = 0;
    need_out_focus = false;
    need_flash_paper = false;
    focus_type = 0;
    focus_record_message_unique_id = 0;
    choose_task_fall_timing = 0;
    choose_task_start_time = 0;

    //获取任务信息
    TodoItem* chose_todo;
    chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->focusing_task_id);
    focus_type = chose_todo->taskType;
    choose_task_fall_timing = chose_todo->fallTiming;
    choose_task_start_time = chose_todo->startTime;
    memset(choose_task_title, 0, sizeof(choose_task_title));
    strcpy(choose_task_title, chose_todo->title);

    //如果是音乐任务
    if(focus_type == 3)
    {
        sscanf(choose_task_title, "Record Task %lu", &focus_record_message_unique_id);
        ESP_LOGI(STATEMACHINE,"Enter FocusTaskState, focus_type: %d, focus_task_id: %d, Record Message Unique ID: %lu", focus_type, get_global_data()->focusing_task_id, focus_record_message_unique_id);
    }
    else
    {
        ESP_LOGI(STATEMACHINE,"Enter FocusTaskState, focus_type: %d, focus_task_id: %d", focus_type, get_global_data()->focusing_task_id);
    }

    //更新屏幕
    lock_lvgl();

    if(focus_type == 1)
    {
        switch_screen(ui_FocusScreen);
        lv_obj_add_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_NoFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoFocusTime, LV_OBJ_FLAG_HIDDEN);
    }
    else if(focus_type == 2)
    {
        switch_screen(ui_FocusScreen);
        lv_obj_add_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_TaskFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus2, LV_OBJ_FLAG_HIDDEN);

        //更新任务描述
        change_focus_task_label(choose_task_title);
    }
    else if(focus_type == 3)
    {
        switch_screen(ui_FocusScreen);
        lv_obj_clear_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_RecordFocus1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_RecordFocus2, LV_OBJ_FLAG_HIDDEN);
    }

    flush_focus_time();
    release_lvgl();

    ControlDriver::Instance()->button3.CallbackLongPress.registerCallback(outfocus);
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    if(need_out_focus) 
    {
        if(elabelUpdateTick % 100 == 0)
        {
            ElabelController::Instance()->stuck_time+=100;
        }
        return;
    }

    if(get_global_data()->m_is_host == 1)
    {
        if(inner_time_countup_ms == 0)
        {
            post_music_info();
        }
        else if(inner_time_countup_ms == 5000)
        {
            get_music_info();
        }
    }
    
    if(elabelUpdateTick % 1000 == 0)
    {
        need_flash_paper = true;
    }
    
    if(elabelUpdateTick % 20 == 0)
    {
        inner_time_countup_ms+=20;
    }


    int remain = choose_task_fall_timing - (get_unix_time() - choose_task_start_time)/1000;
    
    // 只在剩余时间发生变化时判断
    if(remain != last_beep_time)
    {
        // 5分钟整点
        if(remain > 300)
        {
            if(remain % 300 == 0)
            {
                play_focus_music();
                ESP_LOGI(STATEMACHINE,"play long beep:%d", remain);
            }
        }
        // 1分钟整点
        else if(remain > 60)
        {
            if(remain % 60 == 0)
            {
                play_focus_music();
                ESP_LOGI(STATEMACHINE,"play double beep:%d", remain);
            }
        }
        // 10秒整点
        else if(remain > 0)
        {
            if(remain % 10 == 0)
            {
                play_focus_music();
                ESP_LOGI(STATEMACHINE,"play triple beep:%d", remain);
            }
        }
        // 超时后每N秒
        else if(remain <= 0 && get_global_data()->m_device_info.overtime_alert_time > 0)
        {
            int abs_seconds = -remain;
            if(abs_seconds % get_global_data()->m_device_info.overtime_alert_time == 0)
            {
                play_focus_music();
                ESP_LOGI(STATEMACHINE,"play super long beep:%d", remain);
            }
        }
        last_beep_time = remain;
    }

    if(need_flash_paper)
    {
        lock_lvgl();
        flush_focus_time();
        release_lvgl();
        need_flash_paper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{
    //只有在每次进入choosetask的时候或者推出focus的时候需要重置时间
    pOwner->TimeCountdown = (get_global_data()->m_device_info.default_counter_time*60);
    //出focus的时候还要重置选择的是第几个taskNumNum
    pOwner->ChosenTaskNum = 0;
    pOwner->CenterTaskNum = 0;
    ControlDriver::Instance()->button3.CallbackLongPress.unregisterCallback(outfocus);
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}