#include "OperatingTaskState.hpp"
#include "control_driver.hpp"
#include "ssd1680.h"
#include "http.h"
#include "codec.hpp"
#include "Esp_now_client.hpp"

#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

void Time_plus_1()
{
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 60;
    if(time_countdown > MAX_time) time_countdown = 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    ElabelController::Instance()->need_flash_paper = true;
}

void Time_minus_1()
{
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 60;
    if(time_countdown < 0) time_countdown = MAX_time - 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    ElabelController::Instance()->need_flash_paper = true;
}

void Time_plus_5()
{
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 300;
    if(time_countdown > MAX_time) time_countdown = 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    ElabelController::Instance()->need_flash_paper = true;
}

void Time_minus_5()
{
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 300;
    if(time_countdown < 0) time_countdown = MAX_time - 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    ElabelController::Instance()->need_flash_paper = true;
}

void confirm_time()
{
    //会重复触发，所以需要判断
    if(OperatingTaskState::Instance()->is_confirm_time) return;
    if(ElabelController::Instance()->is_confirm_record) 
    {
        if(MCodec::Instance()->recorded_size == 0)
        {
            ESP_LOGW(STATEMACHINE,"is_confirm_record is true, but recorded_size is 0, No need to confirm time");
            return;
        }
        else
        {
            OperatingTaskState::Instance()->is_confirm_time = true;
        }
    }
    else if(ElabelController::Instance()->is_confirm_task)
    {
        OperatingTaskState::Instance()->is_confirm_time = true;
        if(get_global_data()->m_is_host == 1)
        {
            char sstr[12];
            INT_TO_STRING(ElabelController::Instance()->ChosenTaskId, sstr);
            http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
        }
        TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
        ESP_LOGI("OperatingState","enter focus title: %s",chose_todo->title);
    }
}

void record_message()
{
    MCodec::Instance()->stop_play();
    MCodec::Instance()->start_record();
}

void play_message()
{
    MCodec::Instance()->play_record(MCodec::Instance()->record_buffer, MCodec::Instance()->recorded_size);
}

void unconfirm_task()
{
    OperatingTaskState::Instance()->is_not_confirm_task = true;
}

void OperatingTaskState::Init(ElabelController* pOwner)
{
    
}

void OperatingTaskState::Enter(ElabelController* pOwner)
{
    ElabelController::Instance()->need_flash_paper = false;

    auto_reload_time = auto_enter_time;
    is_confirm_time = false;
    is_not_confirm_task = false;

    pOwner->TimeCountdown = TimeCountdownOffset;
    lock_lvgl();
    //加载界面
    switch_screen(ui_OperatingScreen);
    //初始化时间
    char timestr[10] = "00:00";
    uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
    uint8_t minutes = total_seconds / 60;  // 计算分钟数
    uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
    // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
    sprintf(timestr, "%02d:%02d", minutes, seconds);
    set_text_without_change_font(ui_OperatingTime, timestr);
    release_lvgl();
    ESP_LOGI(STATEMACHINE,"Enter OperatingTaskState.");

    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(Time_plus_1);

    ControlDriver::Instance()->button3.CallbackLongPress.registerCallback(confirm_time);

    if(pOwner->is_confirm_record)
    {
        ControlDriver::Instance()->button2.CallbackLongPress.registerCallback(record_message);
        ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(play_message);
        MCodec::Instance()->play_music("speech_guidance");
    }
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
    //当已经确认时间后，不需要再执行
    if(is_confirm_time) return;
    if(pOwner->need_flash_paper)
    {
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_OperatingTime, timestr);
        auto_reload_time = auto_enter_time;
        pOwner->need_flash_paper = false;
    }
}

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(Time_plus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(Time_minus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(Time_plus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(Time_minus_1);
    
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_time);

    if(pOwner->is_confirm_record)
    {
        ControlDriver::Instance()->button2.CallbackLongPress.unregisterCallback(record_message);
        ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(play_message);
        MCodec::Instance()->stop_play();
        MCodec::Instance()->stop_record();
    }
    ESP_LOGI(STATEMACHINE,"Out OperatingTaskState.\n");
}