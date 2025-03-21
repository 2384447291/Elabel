#include "FocusTaskState.hpp"
#include "control_driver.hpp"
#include "network.h"
#include "http.h"
#include "ssd1680.h"
#include "codec.hpp"
#include "Esp_now_client.hpp"


void outfocus()
{
    if(FocusTaskState::Instance()->need_out_focus) return;
    FocusTaskState::Instance()->need_out_focus = true;
    if(FocusTaskState::Instance()->focus_type == 3)
    {
        char sstr[12];
        sprintf(sstr, "%d", get_global_data()->m_focus_state->focus_task_id);
        http_out_focus(sstr,false);
    }
}

void FocusTaskState::Init(ElabelController* pOwner)
{
    
}

void FocusTaskState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter FocusTaskState."); 
    inner_time_countdown_ms = 0;
    inner_time_countdown_s = 0;
    need_out_focus = false;
    focus_type = 0;
    need_flash_paper = false;

    //获取如何进入的focus状态
    focus_type = pOwner->get_focus_type();
    ControlDriver::Instance()->button3.CallbackLongPress.registerCallback(outfocus);

    if(focus_type == 1)
    {
        inner_time_countdown_ms = pOwner->TimeCountdown*1000;
        inner_time_countdown_s = pOwner->TimeCountdown;
        lock_lvgl();
        switch_screen(ui_FocusScreen);
        lv_obj_clear_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_RecordFocus1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_RecordFocus2, LV_OBJ_FLAG_HIDDEN);

        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_RecordFocusTime, timestr);
        release_lvgl();
    }
    if(focus_type == 2)
    {
        inner_time_countdown_ms = pOwner->TimeCountdown*1000;
        inner_time_countdown_s = pOwner->TimeCountdown;
        lock_lvgl();
        switch_screen(ui_FocusScreen);
        lv_obj_add_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_NoFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoFocusTime, LV_OBJ_FLAG_HIDDEN);

        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_NoFocusTime, timestr);
        release_lvgl();
    }
    else if(focus_type == 3)
    {
        //获取是哪个任务进入了focus，这个状态只能由服务器获取
        TodoItem* chose_todo;   
        chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);

        if(chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000 <= 0) pOwner->TimeCountdown = 0;
        else pOwner->TimeCountdown = chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000;
        chose_todo->fallTiming = pOwner->TimeCountdown;

        //将时间转换为毫秒
        inner_time_countdown_ms = pOwner->TimeCountdown*1000;
        inner_time_countdown_s = pOwner->TimeCountdown;
        
        lock_lvgl();
        switch_screen(ui_FocusScreen);
        lv_obj_add_flag(ui_RecordFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoFocus, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_TaskFocusWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskFocus2, LV_OBJ_FLAG_HIDDEN);

        set_text_without_change_font(ui_FocusTask, chose_todo->title);

        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_TaskFocusTime, timestr);
        release_lvgl();
    }
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    if(need_out_focus) return;
    if(elabelUpdateTick % 1000 == 0)
    {
        inner_time_countdown_s--;
        need_flash_paper = true;
    }

    if(elabelUpdateTick % 20 == 0)
    {
        inner_time_countdown_ms-=20;
    }

    //当时间大于5分钟，每5min响一次
    if(inner_time_countdown_ms >= 300000 )
    {
        if(inner_time_countdown_ms % 300000 == 0)
        {
            play_focus_music();
            ESP_LOGI(STATEMACHINE,"play long beep:%d",inner_time_countdown_ms);
        }
    }
    //当时间大于1分钟，每1min响一次
    else if(inner_time_countdown_ms >= 60000)
    {
        if(inner_time_countdown_ms % 60000 == 0)
        {
            play_focus_music();
            ESP_LOGI(STATEMACHINE,"play double beep:%d",inner_time_countdown_ms);
        }
    }
    //当时间大于0，每10s响一次
    else if(inner_time_countdown_ms >= 0)
    {
        if(inner_time_countdown_ms % 10000 == 0)
        {
            play_focus_music();
            ESP_LOGI(STATEMACHINE,"play triple beep:%d",inner_time_countdown_ms);
        }
    }
    //当时间小于0，每10s响一次
    else if(inner_time_countdown_ms < 0)
    {
        if((-inner_time_countdown_ms) % 10000 == 0)
        {
            play_focus_music();
            ESP_LOGI(STATEMACHINE,"play super long beep:%d",inner_time_countdown_ms);
        }
    }

    if(need_flash_paper)
    {
        flush_focus_time();
        need_flash_paper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{
    //只有在每次进入choosetask的时候或者推出focus的时候需要重置时间
    pOwner->TimeCountdown = TimeCountdownOffset;
    //出focus的时候还要重置选择的是第几个taskNumNum
    pOwner->ChosenTaskNum = 0;
    ControlDriver::Instance()->button3.CallbackLongPress.unregisterCallback(outfocus);
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}