#include "OperatingTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "ssd1680.h"
#include "http.h"

#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

#define auto_enter_time 500;

void confirm_time()
{
    OperatingTaskState::Instance()->is_confirm_time = true;
    char sstr[12];
    INT_TO_STRING(ElabelController::Instance()->ChosenTaskId, sstr);
    TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
    ESP_LOGI("OperatingState","enter focus title: %s",chose_todo->title);
    http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
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
    //习惯性操作
    ElabelController::Instance()->need_flash_paper = false;

    auto_reload_time = auto_enter_time;
    update_lock = 0;
    is_confirm_time = false;
    is_not_confirm_task = false;
    pOwner->TimeCountdown = TimeCountdownOffset;
    set_partial_area(TIME_SET);
    
    lock_lvgl();
    //加载界面
    lv_scr_load(uic_FocusScreen);
    //2是倒计时的ui
    _ui_flag_modify(ui_Container4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    //4是选择计时时间的ui
    _ui_flag_modify(ui_Container2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    //初始化时间
    char timestr[10] = "00:00";
    uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
    uint8_t minutes = total_seconds / 60;  // 计算分钟数
    uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
    // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
    sprintf(timestr, "%02d:%02d", minutes, seconds);
    for(int i = 0;i<lv_obj_get_child_cnt(ui_Container2);i++)
    {
        lv_obj_t *uic_textlabel = lv_obj_get_child(ui_Container2, i);
        lv_label_set_text(uic_textlabel, timestr);
    }
    release_lvgl();

    reset_EncoderValue();
    last_encoder_value = get_EncoderValue();
    register_button_down_short_press_call_back(unconfirm_task);
    register_button_up_short_press_call_back(unconfirm_task);
    register_button_down_long_press_call_back(confirm_time);
    register_button_up_long_press_call_back(confirm_time);
    ESP_LOGI(STATEMACHINE,"Enter OperatingTaskState.");
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
    //刷新锁
    if(update_lock!=0) update_lock --;
    //自动触发进入focus
    if(auto_reload_time!=0) auto_reload_time--;
    else confirm_time();

    //限制编码器的值
    int reciver_value = second_perencoder*get_EncoderValue() + TimeCountdownOffset;
    if(reciver_value < 0)
    {
        set_EncoderValue((MAX_time - TimeCountdownOffset)/second_perencoder);
    }
    else if(reciver_value > MAX_time)
    {
        set_EncoderValue((0 - TimeCountdownOffset)/second_perencoder);
    }

    pOwner->TimeCountdown = second_perencoder*get_EncoderValue() + TimeCountdownOffset;

    if(get_EncoderValue() != last_encoder_value && update_lock == 0)
    {
        pOwner->need_flash_paper = true;
    }

    if(pOwner->need_flash_paper)
    {
        last_encoder_value = get_EncoderValue();
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);

        for(int i = 0;i<lv_obj_get_child_cnt(ui_Container2);i++)
        {
            lv_obj_t *uic_textlabel = lv_obj_get_child(ui_Container2, i);
            lv_label_set_text(uic_textlabel, timestr);
        }
        //20ms一次更新，50次刚好1s
        update_lock = 50;
        //20ms一次更新，500次刚好1s
        auto_reload_time = auto_enter_time;
        pOwner->need_flash_paper = false;
    }
}

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    unregister_button_down_short_press_call_back(unconfirm_task);
    unregister_button_up_short_press_call_back(unconfirm_task);
    unregister_button_down_long_press_call_back(confirm_time);
    unregister_button_up_long_press_call_back(confirm_time);
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}