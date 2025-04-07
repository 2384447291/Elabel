#ifndef OPERATINGTASKSTATE_HPP
#define OPERATINGTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "http.h"
#include "Esp_now_slave.hpp"

#define CONFIRM_TASK_TIME 15
#define RECONFIRM_TASK_TIME 3

typedef enum
{
    default_task_process,
    Task_confirm_process,
    Task_reconfirm_process,
    finish_task_process,
} Task_process;

class OperatingTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static OperatingTaskState* Instance()
    {
        static OperatingTaskState instance;
        return &instance;
    }

    Task_process task_process = default_task_process;
    bool button_choose_task_confirm_left = false;
    uint8_t task_confirm_countdown = CONFIRM_TASK_TIME;
    uint8_t task_reconfirm_countdown = RECONFIRM_TASK_TIME;
    bool need_flash_paper = false;
    bool need_out_state = false;

    void enter_screen_confirm_task()
    {
        ESP_LOGI("OperatingTaskState", "enter_screen_confirm_task");
        task_process = Task_confirm_process;
        task_confirm_countdown = CONFIRM_TASK_TIME;
        button_choose_task_confirm_left = false;
 
        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_add_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_TaskName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskOperatetTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskOperateButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_TaskOperateCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_TaskOperateStart, LV_STATE_PRESSED );

        lv_obj_add_flag(ui_TaskOperateAutoGuide, LV_OBJ_FLAG_HIDDEN);

        //设置任务名称
        TodoItem* chose_todo;   
        chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
        set_text_without_change_font(ui_TaskName, chose_todo->title);

        //设置设定的时间
        char timestr[10] = "00:00";
        uint32_t total_seconds = ElabelController::Instance()->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_TaskOperatetTime, timestr);
        release_lvgl();
    }

    void enter_screen_reconfirm_task()
    {
        ESP_LOGI("OperatingTaskState", "enter_screen_reconfirm_task");
        task_process = Task_reconfirm_process;
        task_reconfirm_countdown = RECONFIRM_TASK_TIME;
 
        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_add_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_TaskName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TaskOperatetTime, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperateButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(ui_TaskOperateCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_TaskOperateStart, LV_STATE_PRESSED );

        lv_obj_clear_flag(ui_TaskOperateAutoGuide, LV_OBJ_FLAG_HIDDEN);

        char time_str[30];
        sprintf(time_str, "Auto start in %d secs", task_reconfirm_countdown);
        set_text_without_change_font(ui_TaskOperateAutoGuide, time_str);
        release_lvgl();
    }

    void enter_screen_finish_task()
    {
        task_process = finish_task_process;
        TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
        //并且发送enterfocus的指令
        if(get_global_data()->m_is_host == 1)
        {
            char sstr[12];
            sprintf(sstr, "%d", ElabelController::Instance()->ChosenTaskId);
            http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
        }
        else if(get_global_data()->m_is_host == 2)
        {
            focus_message_t focus_message = pack_focus_message(2, ElabelController::Instance()->TimeCountdown, ElabelController::Instance()->ChosenTaskId, chose_todo->title);
            EspNowSlave::Instance()->slave_send_espnow_http_enter_focus_task(focus_message);
        }
        ESP_LOGI("OperatingState","enter focus title: %s, time: %d",chose_todo->title,ElabelController::Instance()->TimeCountdown);
    }
};
#endif