#include "OperatingTimerState.hpp"
#include "control_driver.hpp"
static void Time_plus_1()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 60;
    if(time_countdown > MAX_time) time_countdown = 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTimeState::Instance()->need_flash_paper = true;
}

static void Time_minus_1()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 60;
    if(time_countdown < 0) time_countdown = MAX_time - 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTimeState::Instance()->need_flash_paper = true;
}

static void Time_plus_5()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 300;
    if(time_countdown > MAX_time) time_countdown = 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTimeState::Instance()->need_flash_paper = true;
}

static void Time_minus_5()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 300;
    if(time_countdown < 0) time_countdown = MAX_time - 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingTimeState::Instance()->need_flash_paper = true;
}

void change_time_confirm_button_choice()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    ESP_LOGI("OperatingTimeState", "change_time_confirm_button_choice");
    OperatingTimeState::Instance()->button_choose_time_confirm_left = !OperatingTimeState::Instance()->button_choose_time_confirm_left;
    OperatingTimeState::Instance()->need_flash_paper = true;
}

void confirm_time_confirm_button_choice()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    ESP_LOGI("OperatingTimeState", "confirm_time_confirm_button_choice");
    //如果选择了cancel，从哪来回哪去
    if(OperatingTimeState::Instance()->button_choose_time_confirm_left)
    {
        OperatingTimeState::Instance()->need_out_state = true;
    }
    //如果选择了finish，结束录音
    else
    {
        OperatingTimeState::Instance()->enter_screen_reconfirm_time();
    }
}

void jump_to_record()
{
    if(OperatingTimeState::Instance()->time_process != Time_confirm_process) return;
    OperatingTimeState::Instance()->need_jump_to_record = true;
}

static void reset_auto_start()
{
    if(OperatingTimeState::Instance()->time_process == Time_reconfirm_process) OperatingTimeState::Instance()->enter_screen_confirm_time();
    if(OperatingTimeState::Instance()->time_process == Time_confirm_process)  OperatingTimeState::Instance()->time_confirm_countdown = CONFIRM_TIMER_TIME;
}

void OperatingTimeState::Init(ElabelController* pOwner)
{
    
}

void OperatingTimeState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter OperatingTimeState.\n");
    time_process = Time_confirm_process;
    button_choose_time_confirm_left = true;
    time_confirm_countdown = CONFIRM_TIMER_TIME;
    time_reconfirm_countdown = RECONFIRM_TIMER_TIME;
    need_flash_paper = false;
    need_out_state = false;
    need_jump_to_record = false;
    enter_screen_confirm_time();
    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(Time_plus_1);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_time_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_time_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_time_confirm_button_choice);

    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(jump_to_record);

    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->register_all_button_callback(reset_auto_start);
    //取消确定键的打断效果
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(reset_auto_start);
}

void OperatingTimeState::Execute(ElabelController* pOwner)
{
   //保证推出后不会有其他问题
    if(need_out_state || need_jump_to_record) return;
    if(time_process == Time_confirm_process)
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
            set_text_without_change_font(ui_NoOperatetTime, timestr);

            //重新刷新按钮
            if(button_choose_time_confirm_left)
            {
                lv_obj_add_state(ui_NoOperateChooseCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_NoOperateChooseStart, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_NoOperateChooseCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_NoOperateChooseStart, LV_STATE_PRESSED );
            }
            release_lvgl();
            need_flash_paper = false;
        }
        if(elabelUpdateTick%1000 == 0)
        {
            time_confirm_countdown--;
        }
        if(time_confirm_countdown == 0)
        {
            enter_screen_reconfirm_time();
        }
    }
    else if(time_process == Time_reconfirm_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            time_reconfirm_countdown--;
            lock_lvgl();
            char time_str[30];
            sprintf(time_str, "Auto start in %d secs", time_reconfirm_countdown);
            set_text_without_change_font(ui_NoOperateAutoGuide, time_str);
            release_lvgl();
        }
        if(time_reconfirm_countdown == 0)
        {
            enter_screen_finish_time();
        }
    }
}

void OperatingTimeState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(Time_plus_1);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_time_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_time_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_time_confirm_button_choice);

    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(jump_to_record);

    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->unregister_button_callback(reset_auto_start);
    ESP_LOGI(STATEMACHINE,"Out OperatingTimeState.\n");
}
