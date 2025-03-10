#include "FocusTaskState.hpp"
#include "control_driver.hpp"
#include "network.h"
#include "http.h"
#include "ssd1680.h"
#include "codec.hpp"
#include "Esp_now_client.hpp"


#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

void outfocus()
{
    if(ElabelController::Instance()->is_confirm_task)
    {
        if(get_global_data()->m_is_host == 1)
        {
            char sstr[12];
            INT_TO_STRING(get_global_data()->m_focus_state->focus_task_id, sstr);
            http_out_focus(sstr,false);
        }
        TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
        ESP_LOGI("FocusState","out focus title: %s",chose_todo->title);
    }

    FocusTaskState::Instance()->need_out_focus = true;
    ElabelController::Instance()->TimeCountdown = 0;
}

void FocusTaskState::Init(ElabelController* pOwner)
{
    
}

void FocusTaskState::Enter(ElabelController* pOwner)
{
    pOwner->need_flash_paper = false;
    need_out_focus = false;
    if(pOwner->is_confirm_record)
    {
        inner_time_countdown = pOwner->TimeCountdown*1000;
        lock_lvgl();
        switch_screen(ui_FocusScreen);
        _ui_flag_modify(ui_FocusTask, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_speachbar, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_FocusTime, timestr);
        release_lvgl();
    }
    else if(pOwner->is_confirm_task)
    {
        //获取是哪个任务进入了focus，这个状态只能由服务器获取
        TodoItem* chose_todo;   
        chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);

        if(get_global_data()->m_is_host == 1)
        {
            if(chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000 <= 0) pOwner->TimeCountdown = 0;
            else pOwner->TimeCountdown = chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000;
            chose_todo->fallTiming = pOwner->TimeCountdown;
        }
        else if(get_global_data()->m_is_host == 2)
        {
            pOwner->TimeCountdown = chose_todo->fallTiming;
        }

        //将时间转换为毫秒
        inner_time_countdown = pOwner->TimeCountdown*1000;
        lock_lvgl();
        _ui_flag_modify(ui_FocusTask, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_speachbar, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        switch_screen(ui_FocusScreen);
        set_text_without_change_font(ui_FocusTask, chose_todo->title);

        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_FocusTime, timestr);
        release_lvgl();
    }
    ESP_LOGI(STATEMACHINE,"Enter FocusTaskState."); 

    ControlDriver::Instance()->button3.CallbackLongPress.registerCallback(outfocus);
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    if(need_out_focus) return;
    if(pOwner->TimeCountdown > 0 && elabelUpdateTick % 1000 == 0)
    {
        pOwner->TimeCountdown--;
        pOwner->need_flash_paper = true;
    }

    if(elabelUpdateTick % 20 == 0)
    {
        inner_time_countdown-=20;
    }

    //当时间大于5分钟，每5min响一次
    if(inner_time_countdown >= 300000 )
    {
        if(inner_time_countdown % 300000 == 0)
        {
            if(pOwner->is_confirm_record)
            {
                MCodec::Instance()->play_record(MCodec::Instance()->record_buffer,MCodec::Instance()->recorded_size);
            }
            else
            {
                MCodec::Instance()->play_music("bell");
            }
            ESP_LOGI(STATEMACHINE,"play long beep:%d",inner_time_countdown);
        }
    }
    //当时间大于1分钟，每1min响一次
    else if(inner_time_countdown >= 60000)
    {
        if(inner_time_countdown % 60000 == 0)
        {
            if(pOwner->is_confirm_record)
            {
                MCodec::Instance()->play_record(MCodec::Instance()->record_buffer,MCodec::Instance()->recorded_size);
            }
            else
            {
                MCodec::Instance()->play_music("bell");
            }
            ESP_LOGI(STATEMACHINE,"play double beep:%d",inner_time_countdown);
        }
    }
    //当时间大于0，每10s响一次
    else if(inner_time_countdown >= 0)
    {
        if(inner_time_countdown % 10000 == 0)
        {
            if(pOwner->is_confirm_record)
            {
                MCodec::Instance()->play_record(MCodec::Instance()->record_buffer,MCodec::Instance()->recorded_size);
            }
            else
            {
                MCodec::Instance()->play_music("bell");
            }
            ESP_LOGI(STATEMACHINE,"play triple beep:%d",inner_time_countdown);
        }
    }
    //当时间小于0，每10s响一次
    else if((-inner_time_countdown > 0))
    {
        if((-inner_time_countdown) % 10000 == 0)
        {
            if(pOwner->is_confirm_record)
            {
                MCodec::Instance()->play_record(MCodec::Instance()->record_buffer,MCodec::Instance()->recorded_size);
            }
            else
            {
                MCodec::Instance()->play_music("bell");
            }
            ESP_LOGI(STATEMACHINE,"play super long beep:%d",inner_time_countdown);
        }
    }

    if(pOwner->need_flash_paper)
    {
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_FocusTime, timestr);
        pOwner->need_flash_paper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{

    ControlDriver::Instance()->button3.CallbackLongPress.unregisterCallback(outfocus);
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}