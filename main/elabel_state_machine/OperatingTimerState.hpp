#ifndef OPERATINGTIMERSTATE_HPP
#define OPERATINGTIMERSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Esp_now_slave.hpp"

#define CONFIRM_TIMER_TIME 15
#define RECONFIRM_TIMER_TIME 3

typedef enum
{
    default_time_process,
    Time_confirm_process,
    Time_reconfirm_process,
    finish_time_process,
} Time_process;

class OperatingTimeState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static OperatingTimeState* Instance()
    {
        static OperatingTimeState instance;
        return &instance;
    }

    Time_process time_process = default_time_process;
    bool button_choose_time_confirm_left = false;
    uint8_t time_confirm_countdown = CONFIRM_TIMER_TIME;
    uint8_t time_reconfirm_countdown = RECONFIRM_TIMER_TIME;
    bool need_flash_paper = false;
    bool need_out_state = false;
    bool need_jump_to_record = false;

    void enter_screen_confirm_time()
    {
        ESP_LOGI("OperatingTimeState", "enter_screen_confirm_time");
        time_process = Time_confirm_process;
        time_confirm_countdown = CONFIRM_TIMER_TIME;
        button_choose_time_confirm_left = false;
 
        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_add_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_NoOperatetTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoOperateChooseButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_NoOperateChooseCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_NoOperateChooseStart, LV_STATE_PRESSED );

        lv_obj_add_flag(ui_NoOperateAutoGuide, LV_OBJ_FLAG_HIDDEN);

        //设置设定的时间
        char timestr[10] = "00:00";
        uint32_t total_seconds = ElabelController::Instance()->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_NoOperatetTime, timestr);
        release_lvgl();
    }

    void enter_screen_reconfirm_time()
    {
        ESP_LOGI("OperatingTimeState", "enter_screen_reconfirm_time");
        time_process = Time_reconfirm_process;
        time_reconfirm_countdown = RECONFIRM_TIMER_TIME;
 
        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_add_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_NoOperatetTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperateChooseButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(ui_NoOperateChooseCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_NoOperateChooseStart, LV_STATE_PRESSED );

        lv_obj_clear_flag(ui_NoOperateAutoGuide, LV_OBJ_FLAG_HIDDEN);

        char time_str[30];
        sprintf(time_str, "Auto start in %d secs", time_reconfirm_countdown);
        set_text_without_change_font(ui_NoOperateAutoGuide, time_str);
        release_lvgl();
    }

    void enter_screen_finish_time()
    {
        ESP_LOGI("OperatingTimeState", "enter_screen_finish_time");
        time_process = finish_time_process;
        if(get_global_data()->m_is_host == 1)
        {
            //下面的操作模仿了收到了mqtt和http
            //刷新一下focus状态，真正的判断是否有entertask的操作是在http_get_todo_list中
            get_global_data()->m_focus_state->is_focus = 0;
            get_global_data()->m_focus_state->focus_task_id = 0;

            TodoItem todo;
            cleantodoItem(&todo);

            todo.foucs_type = 1;
            todo.fallTiming = ElabelController::Instance()->TimeCountdown;
            todo.id = 0;
            todo.isFocus = 1;
            todo.startTime = get_unix_time();
            char title[20] = "Pure Time Task";
            todo.title = title;
            clean_todo_list(get_global_data()->m_todo_list);
            add_or_update_todo_item(get_global_data()->m_todo_list, todo);
            set_task_list_state(firmware_need_update);
        }
        else if(get_global_data()->m_is_host == 2)
        {
            char title[20] = "Pure Time Task";
            focus_message_t focus_message = pack_focus_message(1, ElabelController::Instance()->TimeCountdown, 0, title);
            EspNowSlave::Instance()->slave_send_espnow_http_enter_focus_task(focus_message);
        }
    }
};
#endif