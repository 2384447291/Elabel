#include "SleepFocusState.hpp"
#include "control_driver.hpp"
#include "global_time.h"
#include "global_draw.h"

void sleep_outfocus()
{
    if(SleepFocusState::Instance()->need_out_focus) return;
    play_finish_task_sound();
    SleepFocusState::Instance()->need_out_focus = true;

    focus_message_t focus_message = pack_focus_message(0, 0, 0,get_global_data()->focusing_task_id, (char*)"");
    EspNowSlave::Instance()->slave_send_espnow_http_out_focus_task(focus_message);
}


void SleepFocusState::Init(ElabelController* pOwner)
{
}

void SleepFocusState::Enter(ElabelController* pOwner)
{
    // 进入focus的时候，重置卡死时间,这个代表的是outfocus的倒计时
    pOwner->stuck_time = 0;
    
    //同步时间戳
    EspNowSlave::Instance()->slave_send_espnow_http_get_time();
    vTaskDelay(pdMS_TO_TICKS(100));

    //是否需要跳出该状态
    need_out_state = false;
    //是否要退出状态
    need_out_focus = false;
    //下一次唤醒的时间
    next_wake_up_time = 0;

    //当前focus的类型1是纯时间，2是task，3是record
    focus_type = 0;
    //当前focus的record_message_unique_id,从任务名字中获取，需要和mcodec里的对应
    focus_record_message_unique_id = 0;

    //任务描述
    choose_task_fall_timing = 0;
    choose_task_start_time = 0;
    memset(choose_task_title, 0, sizeof(choose_task_title));

    is_sleep_focus = false;
    sleep_count = SLEEP_FOCUS_COUTDOWN;

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
        ESP_LOGI(STATEMACHINE,"Enter SleepFocusTaskState, focus_type: %u, focus_task_id: %d, Record Message Unique ID: %lu, Mcodec Record Message Unique ID: %lu", focus_type, get_global_data()->focusing_task_id, focus_record_message_unique_id, MCodec::Instance()->record_message_unique_id);
    }
    else
    {
        ESP_LOGI(STATEMACHINE,"Enter SleepFocusTaskState, focus_type: %u, focus_task_id: %d", focus_type, get_global_data()->focusing_task_id);
    }

    lock_lvgl();
    switch_screen(ui_SleepFocusScreen);

    if(focus_type == 3)
    {
        //如果是自己发起的record任务
        if(focus_record_message_unique_id == MCodec::Instance()->record_message_unique_id)
        {
            lv_obj_clear_flag(ui_SleepTaskFocusTime, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_SleepTaskFocusName, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_SleepTimeFocusTime, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_SleepRecordFocusName, LV_OBJ_FLAG_HIDDEN);
        }
        //如果不是自己发起的任务
        else
        {
            lv_obj_add_flag(ui_SleepTaskFocusTime, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_SleepTaskFocusName, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_SleepTimeFocusTime, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_SleepRecordFocusName, LV_OBJ_FLAG_HIDDEN);
        }

    }
    //如果是task任务
    else if(focus_type == 2)
    {
        lv_obj_clear_flag(ui_SleepTaskFocusTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_SleepTaskFocusName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SleepTimeFocusTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SleepRecordFocusName, LV_OBJ_FLAG_HIDDEN);
        //设置倒计时任务长度
        set_text_without_change_font(ui_SleepTaskFocusName, choose_task_title);
    }
    else if(focus_type == 1)
    {
        lv_obj_add_flag(ui_SleepTaskFocusTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SleepTaskFocusName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_SleepTimeFocusTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SleepRecordFocusName, LV_OBJ_FLAG_HIDDEN);
    }

    set_time_str_and_process(is_sleep_focus);
    release_lvgl();

    ControlDriver::Instance()->button3.CallbackLongPress.registerCallback(sleep_outfocus);
}
void SleepFocusState::Execute(ElabelController* pOwner)
{
    if(need_out_focus) 
    {
        if(elabelUpdateTick % 100 == 0)
        {
            ElabelController::Instance()->stuck_time+=100;
        }
        return;
    }

    if(!is_sleep_focus && !need_out_focus)
    {
        if(elabelUpdateTick % 1000 == 0)
        {
            sleep_count-=1000;
            if(sleep_count == 0)
            {
                is_sleep_focus = true;
                lock_lvgl();
                set_time_str_and_process(is_sleep_focus); 
                release_lvgl();
                vTaskDelay(pdMS_TO_TICKS(WAITING_BEFORE_SLEEP_TIME));

                //关闭其他额外线程
                ControlDriver::Instance()->stop_button_check_task();
                suspend_gui();

                //进入睡眠
                EspNowSlave::Instance()->slave_send_espnow_http_sleep_request();
                EspNowSlave::Instance()->sleep_sync_flag = 0;
                start_sleep();
            }
            else
            {
                lock_lvgl();
                set_time_str_and_process(is_sleep_focus); 
                release_lvgl();
            }
        }
    }


    
}

void SleepFocusState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button3.CallbackLongPress.unregisterCallback(sleep_outfocus);
    BatteryManager::Instance()->setPowerState(true);
    ESP_ERROR_CHECK(esp_wifi_start());
    EspNowSlave::Instance()->resume_espnow();

    ControlDriver::Instance()->start_button_check_task();
    resume_gui();
    EspNowSlave::Instance()->sleep_sync_flag = 3;
    vTaskDelay(pdMS_TO_TICKS(500));
}

