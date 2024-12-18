#include "FocusTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "network.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "http.h"
#include "ssd1680.h"

#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

void outfocus()
{
    FocusTaskState::Instance()->is_out_focus = true;
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
    //习惯性操作
    pOwner->need_flash_paper = false;
    is_out_focus = false;
    set_partial_area(TIME_CHANGE);
    buzzer_state = 0;
    
    //获取是哪个任务进入了focus，这个状态只能由服务器获取
    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
    pOwner->TimeCountdown = chose_todo->fallTiming;
    falling_time = chose_todo->fallTiming;

    lock_lvgl();
    lv_scr_load(uic_FocusScreen);
    _ui_flag_modify(ui_Container2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Container4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    for(int i=0;i<lv_obj_get_child_cnt(ui_Container5);i++)
    {
        lv_obj_t *child = lv_obj_get_child(ui_Container5, i);
        lv_label_set_text(child, chose_todo->title);
    }
    char timestr[10] = "00:00";
    uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
    uint8_t minutes = total_seconds / 60;  // 计算分钟数
    uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
    // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
    sprintf(timestr, "%02d:%02d", minutes, seconds);
    for(int i =0;i<lv_obj_get_child_cnt(ui_Container6);i++)
    {
        lv_obj_t *child = lv_obj_get_child(ui_Container6, i);
        lv_label_set_text(child, timestr);
    }
    release_lvgl();
    
    register_button_down_long_press_call_back(outfocus);
    register_button_up_long_press_call_back(outfocus);
    ESP_LOGI(STATEMACHINE,"Enter FocusTaskState."); 

    //设置声音
    //如果大于5分钟，每5分钟响一次, 时长1s
    if (pOwner->TimeCountdown > 300)
    {
        update_buzzer_state(pOwner->TimeCountdown, 300, 1, 0, 1);
        buzzer_state = 4;
        ESP_LOGI("buzzer","init out 5min.");
    } 
    else if(pOwner->TimeCountdown > 60) 
    {
        update_buzzer_state(pOwner->TimeCountdown, 60, 2, 0.2, 0.5);
        buzzer_state = 3;
        ESP_LOGI("buzzer","init in 1-5min.");
    }
    else if(pOwner->TimeCountdown > 0) 
    {
        update_buzzer_state(pOwner->TimeCountdown, 10, 3, 0.2, 0.5);
        buzzer_state = 2;
        ESP_LOGI("buzzer","init in 0-1min.");
    }
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    //设置声音
    //如果大于5分钟，每5分钟响一次, 时长1s
    if (pOwner->TimeCountdown == 290 && buzzer_state != 3)
    {
        update_buzzer_state(pOwner->TimeCountdown, 60, 2, 0.2, 0.5);
        buzzer_state = 3;
        ESP_LOGI("buzzer","enter 5 min.");
    }
    //如果大于1分钟，每1分钟响两次， 时长0.2s
    else if (pOwner->TimeCountdown == 55 && buzzer_state != 2)
    {
        update_buzzer_state(pOwner->TimeCountdown, 10, 3, 0.2, 0.5);
        buzzer_state = 2;
        ESP_LOGI("buzzer","enter 1 min.");
    }
    //如果没有超时，每10秒响三次， 时长0.2s
    else if (pOwner->TimeCountdown == 0 && buzzer_state != 1)
    {
        update_buzzer_state(0, 10, 1 , 0, 2);
        buzzer_state = 1;
        ESP_LOGI("buzzer","over time.");
    }
    //设置灯效
    if(pOwner->TimeCountdown!=0) set_todo_time_up(pOwner->TimeCountdown, falling_time);
    else set_led_state(device_out_time);

    if(pOwner->TimeCountdown > 0 && elabelUpdateTick % 1000 == 0)
    {
        pOwner->TimeCountdown--;
        pOwner->need_flash_paper = true;
    }

    if(pOwner->need_flash_paper)
    {
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        for(int i =0;i<lv_obj_get_child_cnt(ui_Container6);i++)
        {
            lv_obj_t *child = lv_obj_get_child(ui_Container6, i);
            lv_label_set_text(child, timestr);
        }
        pOwner->need_flash_paper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{
    unregister_button_down_long_press_call_back(outfocus);
    unregister_button_up_long_press_call_back(outfocus);
    //关闭buzzer
    update_buzzer_state(0, 0, 0, 0, 0);
    //关闭等效
    set_led_state(no_light);
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}