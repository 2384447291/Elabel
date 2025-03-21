#include "OperatingTaskState.hpp"
#include "control_driver.hpp"
#include "ssd1680.h"
#include "http.h"
#include "codec.hpp"
#include "Esp_now_client.hpp"

static void Time_plus_1()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 60;
    if(time_countdown > MAX_time) time_countdown = 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTaskState::Instance()->need_flash_paper = true;
}

static void Time_minus_1()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 60;
    if(time_countdown < 0) time_countdown = MAX_time - 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTaskState::Instance()->need_flash_paper = true;
}

static void Time_plus_5()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 300;
    if(time_countdown > MAX_time) time_countdown = 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTaskState::Instance()->need_flash_paper = true;
}

static void Time_minus_5()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 300;
    if(time_countdown < 0) time_countdown = MAX_time - 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTaskState::Instance()->need_flash_paper = true;
}

void change_task_confirm_button_choice()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    ESP_LOGI("OperatingTaskState", "change_button_choice");
    OperatingTaskState::Instance()->button_choose_task_confirm_left = !OperatingTaskState::Instance()->button_choose_task_confirm_left;
    OperatingTaskState::Instance()->need_flash_paper = true;
}

void confirm_task_confirm_button_choice()
{
    if(OperatingTaskState::Instance()->task_process != Task_confirm_process) return;
    ESP_LOGI("OperatingTaskState", "confirm_task_confirm_button_choice");
    //如果选择了cancel，从哪来回哪去
    if(OperatingTaskState::Instance()->button_choose_task_confirm_left)
    {
        OperatingTaskState::Instance()->need_out_state = true;
    }
    //如果选择了finish，结束录音
    else
    {
        OperatingTaskState::Instance()->enter_screen_reconfirm_task();
    }
}

static void reset_auto_start()
{
    if(OperatingTaskState::Instance()->task_process == Task_reconfirm_process) OperatingTaskState::Instance()->enter_screen_confirm_task();
    if(OperatingTaskState::Instance()->task_process == Task_confirm_process)  OperatingTaskState::Instance()->task_confirm_countdown = CONFIRM_TASK_TIME;
}

void OperatingTaskState::Init(ElabelController* pOwner)
{
    
}

void OperatingTaskState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter OperatingTaskState.\n");
    task_process = Task_confirm_process;
    button_choose_task_confirm_left = true;
    task_confirm_countdown = CONFIRM_TASK_TIME;
    task_reconfirm_countdown = RECONFIRM_TASK_TIME;
    need_flash_paper = false;
    need_out_state = false;
    enter_screen_confirm_task();
    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(Time_plus_1);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_task_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_task_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_task_confirm_button_choice);

    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->register_all_button_callback(reset_auto_start);
    //取消确定键的打断效果
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(reset_auto_start);
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
   //保证推出后不会有其他问题
    if(need_out_state) return;
    if(task_process == Task_confirm_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            //设置设定的时间
            char timestr[10] = "00:00";
            uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
            uint8_t minutes = total_seconds / 60;  // 计算分钟数
            uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
            sprintf(timestr, "%02d:%02d", minutes, seconds);
            set_text_without_change_font(ui_TaskOperatetTime, timestr);

            //重新刷新按钮
            if(button_choose_task_confirm_left)
            {
                lv_obj_add_state(ui_TaskOperateCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_TaskOperateStart, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_TaskOperateCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_TaskOperateStart, LV_STATE_PRESSED );
            }
            release_lvgl();
            need_flash_paper = false;
        }
        if(elabelUpdateTick%1000 == 0)
        {
            task_confirm_countdown--;
        }
        if(task_confirm_countdown == 0)
        {
            enter_screen_reconfirm_task();
        }
    }
    else if(task_process == Task_reconfirm_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            task_reconfirm_countdown--;
            lock_lvgl();
            char time_str[30];
            sprintf(time_str, "Auto start in %d secs", task_reconfirm_countdown);
            set_text_without_change_font(ui_TaskOperateAutoGuide, time_str);
            release_lvgl();
        }
        if(task_reconfirm_countdown == 0)
        {
            task_process = finish_task_process;
            //并且发送enterfocus的指令
            char sstr[12];
            sprintf(sstr, "%d", ElabelController::Instance()->ChosenTaskId);
            http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
            TodoItem* chose_todo = find_todo_by_id(get_global_data()->m_todo_list, ElabelController::Instance()->ChosenTaskId);
            ESP_LOGI("OperatingState","enter focus title: %s, time: %d",chose_todo->title,ElabelController::Instance()->TimeCountdown);
        }
    }
}

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(Time_plus_1);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_task_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_task_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_task_confirm_button_choice);

    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->unregister_button_callback(reset_auto_start);
    ESP_LOGI(STATEMACHINE,"Out OperatingTaskState.\n");
}