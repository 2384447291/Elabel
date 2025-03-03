#include "OperatingTaskState.hpp"
#include "control_driver.hpp"
#include "ssd1680.h"
#include "http.h"
#include "Esp_now_client.hpp"


#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

#define auto_enter_time 1000; //1000次20ms，刚好20s

void confirm_time()
{
    OperatingTaskState::Instance()->is_confirm_time = true;
    OperatingTaskState::Instance()->slave_unique_id = esp_random() & 0xFF;
    if(get_global_data()->m_is_host == 1)
    {
        char sstr[12];
        INT_TO_STRING(ElabelController::Instance()->ChosenTaskId, sstr);
        http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
    }
    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
    ESP_LOGI("OperatingState","enter focus title: %s",chose_todo->title);
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
    is_confirm_time = false;
    ElabelController::Instance()->need_flash_paper = false;

    auto_reload_time = auto_enter_time;
    update_lock = 0;
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
    // ControlDriver::Instance()->resetEncoderValue();
    // last_encoder_value = ControlDriver::Instance()->getEncoderValue();
    ESP_LOGI(STATEMACHINE,"Enter OperatingTaskState.");
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
    // if(is_confirm_time) 
    // {
    //     if(get_global_data()->m_is_host == 2 && elabelUpdateTick % Esp_Now_Send_Interval == 0)
    //     {
    //         EspNowSlave::Instance()->send_enter_focus_message(ElabelController::Instance()->ChosenTaskId,ElabelController::Instance()->TimeCountdown,slave_unique_id);
    //     }
    //     return;
    // }
    // //刷新锁
    // if(update_lock!=0) update_lock --;
    // //自动触发进入focus
    // if(auto_reload_time!=0) auto_reload_time--;
    // else confirm_time();

    // //限制编码器的值
    // int reciver_value = second_perencoder*ControlDriver::Instance()->getEncoderValue() + TimeCountdownOffset;
    // if(reciver_value < 0)
    // {
    //     ControlDriver::Instance()->setEncoderValue((MAX_time - TimeCountdownOffset)/second_perencoder);
    // }
    // else if(reciver_value > MAX_time)
    // {
    //     ControlDriver::Instance()->setEncoderValue((0 - TimeCountdownOffset)/second_perencoder);
    // }

    // pOwner->TimeCountdown = second_perencoder*ControlDriver::Instance()->getEncoderValue() + TimeCountdownOffset;
    
    // if(ControlDriver::Instance()->getEncoderValue() != last_encoder_value && update_lock == 0)
    // {
    //     pOwner->need_flash_paper = true;
    // }

    // if(pOwner->need_flash_paper)
    // {
    //     last_encoder_value = ControlDriver::Instance()->getEncoderValue();
    //     char timestr[10] = "00:00";
    //     uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
    //     uint8_t minutes = total_seconds / 60;  // 计算分钟数
    //     uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
    //     // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
    //     sprintf(timestr, "%02d:%02d", minutes, seconds);
    //     set_text_without_change_font(ui_OperatingTime, timestr);
    //     //20ms一次更新，50次刚好1s
    //     update_lock = 50;
    //     auto_reload_time = auto_enter_time;
    //     pOwner->need_flash_paper = false;
    // }
}

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}