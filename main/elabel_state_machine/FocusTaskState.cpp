#include "FocusTaskState.hpp"
#include "control_driver.hpp"
#include "network.h"
#include "http.h"
#include "ssd1680.h"


#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

void outfocus()
{
    char sstr[12];
    INT_TO_STRING(get_global_data()->m_focus_state->focus_task_id, sstr);
    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
    ESP_LOGI("FocusState","out focus title: %s",chose_todo->title);
    http_out_focus(sstr,false);
    ElabelController::Instance()->TimeCountdown = 0;
}

void FocusTaskState::Init(ElabelController* pOwner)
{
    
}

void FocusTaskState::Enter(ElabelController* pOwner)
{
    pOwner->need_flash_paper = false;
    
    //获取是哪个任务进入了focus，这个状态只能由服务器获取
    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
    if(chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000 <= 0) pOwner->TimeCountdown = 0;
    else pOwner->TimeCountdown = chose_todo->fallTiming - (get_unix_time() - chose_todo->startTime)/1000;

    //将时间转换为毫秒
    inner_time_countdown = pOwner->TimeCountdown*1000;
    lock_lvgl();
    lv_scr_load(ui_FocusScreen);
    set_text(ui_FocusTask, chose_todo->title);

    char timestr[10] = "00:00";
    uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
    uint8_t minutes = total_seconds / 60;  // 计算分钟数
    uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
    // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
    sprintf(timestr, "%02d:%02d", minutes, seconds);
    set_text(ui_FocusTime, timestr);
    release_lvgl();
    ControlDriver::Instance()->ButtonUpLongPress.registerCallback(outfocus);
    ControlDriver::Instance()->ButtonDownLongPress.registerCallback(outfocus);
    ESP_LOGI(STATEMACHINE,"Enter FocusTaskState."); 
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    if(pOwner->TimeCountdown > 0 && elabelUpdateTick % 1000 == 0)
    {
        pOwner->TimeCountdown--;
        pOwner->need_flash_paper = true;
    }

    //20ms调用一次
    inner_time_countdown-=20; 

    //当时间大于5分钟，每5min响一次
    if(inner_time_countdown >= 300000 )
    {
        if(inner_time_countdown % 300000 == 0)
        {
            ESP_LOGI(STATEMACHINE,"play long beep:%d",inner_time_countdown);
            ControlDriver::Instance()->getBuzzer().playLongBeep();
        }
    }
    //当时间大于1分钟，每1min响一次
    else if(inner_time_countdown >= 60000)
    {
        if(inner_time_countdown % 60000 == 0)
        {
            ESP_LOGI(STATEMACHINE,"play double beep:%d",inner_time_countdown);
            ControlDriver::Instance()->getBuzzer().playDoubleBeep();
        }
    }
    //当时间大于0，每10s响一次
    else if(inner_time_countdown >= 0)
    {
        if(inner_time_countdown % 10000 == 0)
        {
            ESP_LOGI(STATEMACHINE,"play triple beep:%d",inner_time_countdown);
            ControlDriver::Instance()->getBuzzer().playTripleBeep();
        }
    }
    //当时间小于0，每10s响一次
    else if((-inner_time_countdown > 0))
    {
        if((-inner_time_countdown) % 10000 == 0)
        {
            ESP_LOGI(STATEMACHINE,"play super long beep:%d",inner_time_countdown);
            ControlDriver::Instance()->getBuzzer().playSuperLongBeep();
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
        set_text(ui_FocusTime, timestr);
        pOwner->need_flash_paper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->ButtonUpLongPress.unregisterCallback(outfocus);
    ControlDriver::Instance()->ButtonDownLongPress.unregisterCallback(outfocus);
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}