#include "OperatingRecorderState.hpp"

static void Time_plus_1()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 60;
    if(time_countdown > MAX_time) time_countdown = 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingRecorderState::Instance()->need_flash_paper = true;
}

static void Time_minus_1()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 60;
    if(time_countdown < 0) time_countdown = MAX_time - 60;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingRecorderState::Instance()->need_flash_paper = true;
}

static void Time_plus_5()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown += 300;
    if(time_countdown > MAX_time) time_countdown = 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingRecorderState::Instance()->need_flash_paper = true;
}

static void Time_minus_5()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    int time_countdown = ElabelController::Instance()->TimeCountdown;
    time_countdown -= 300;
    if(time_countdown < 0) time_countdown = MAX_time - 300;
    ElabelController::Instance()->TimeCountdown = time_countdown;
    OperatingRecorderState::Instance()->need_flash_paper = true;
}

void change_record_confirm_button_choice()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    ESP_LOGI("OperatingRecorderState", "change_button_choice");
    OperatingRecorderState::Instance()->button_choose_record_confirm_left = !OperatingRecorderState::Instance()->button_choose_record_confirm_left;
    OperatingRecorderState::Instance()->need_flash_paper = true;
}

void confirm_record_confirm_button_choice()
{
    if(OperatingRecorderState::Instance()->record_process != Record_time_process) return;
    ESP_LOGI("OperatingRecorderState", "confirm_record_confirm_button_choice");
    //如果选择了cancel，从哪来回哪去
    if(OperatingRecorderState::Instance()->button_choose_record_confirm_left)
    {
        OperatingRecorderState::Instance()->need_out_state = true;
    }
    //如果选择了finish，结束录音
    else
    {
        OperatingRecorderState::Instance()->enter_screen_reconfirm_process();
    }
}

void retry_record()
{
    if(OperatingRecorderState::Instance()->record_process == Record_time_process) 
    OperatingRecorderState::Instance()->enter_screen_record_voice();
}


void finish_record()
{
    if(OperatingRecorderState::Instance()->record_process != Record_voice_process || OperatingRecorderState::Instance()->record_voice_countdown == 0) return;
    OperatingRecorderState::Instance()->enter_screen_confirm_voice();
}

static void reset_auto_start()
{
    if(OperatingRecorderState::Instance()->record_process == Record_reconfirm_process) OperatingRecorderState::Instance()->enter_screen_confirm_voice();
    if(OperatingRecorderState::Instance()->record_process == Record_time_process)  OperatingRecorderState::Instance()->confirm_voice_countdown = CONFIRM_VOICE_TIME;
}

void OperatingRecorderState::Init(ElabelController* pOwner)
{
    
}

void OperatingRecorderState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter OperatingRecorderState.\n");
    record_process = Record_voice_process;
    button_choose_record_confirm_left = true;
    record_voice_countdown = RECORD_TIME;
    confirm_voice_countdown = CONFIRM_VOICE_TIME;
    reconfirm_process_countdown = RECONFIRM_VOICE_TIME;
    need_flash_paper = false;
    need_out_state = false;
    //进入record_voice
    enter_screen_record_voice();
    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(Time_plus_1);

    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(retry_record);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_record_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(change_record_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(change_record_confirm_button_choice);
    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(finish_record);
    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->register_all_button_callback(reset_auto_start);
    //取消确定键的打断效果
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(reset_auto_start);
}

void OperatingRecorderState::Execute(ElabelController* pOwner)
{
    //保证退出后不会有其他问题
    if(need_out_state) return;
    if(record_process == Record_voice_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            record_voice_countdown--;
            lock_lvgl();
            char time_str[20];
            sprintf(time_str, "%ds left", record_voice_countdown);
            set_text_without_change_font(ui_RecordOperateMiddleText, time_str);
            release_lvgl();
        }
        if(record_voice_countdown == 0)
        {
            enter_screen_confirm_voice();
        }
    }
    else if(record_process == Record_time_process)
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
            set_text_without_change_font(ui_RecordOperateTime, timestr);

            //重新刷新按钮
            if(button_choose_record_confirm_left)
            {
                lv_obj_add_state(ui_RecordComfirmCancel, LV_STATE_PRESSED );
                lv_obj_clear_state(ui_RecordComfirmFinish, LV_STATE_PRESSED );
            }
            else
            {
                lv_obj_clear_state(ui_RecordComfirmCancel, LV_STATE_PRESSED );
                lv_obj_add_state(ui_RecordComfirmFinish, LV_STATE_PRESSED );
            }
            release_lvgl();
            need_flash_paper = false;
        }
        if(elabelUpdateTick%1000 == 0)
        {
            confirm_voice_countdown--;
        }
        if(confirm_voice_countdown == 0)
        {
            enter_screen_reconfirm_process();
        }
    }
    else if(record_process == Record_reconfirm_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            reconfirm_process_countdown--;
            lock_lvgl();
            char time_str[30];
            sprintf(time_str, "Auto start in %d secs", reconfirm_process_countdown);
            set_text_without_change_font(ui_RecordOperateDownText, time_str);
            release_lvgl();
        }
        if(reconfirm_process_countdown == 0)
        {
            enter_screen_finish_record();
        }
    }
}

void OperatingRecorderState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(Time_minus_5);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(Time_plus_5);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(Time_minus_1);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(Time_plus_1);

    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(retry_record);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_record_confirm_button_choice);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(change_record_confirm_button_choice);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(change_record_confirm_button_choice);
    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(finish_record);
    //很重要这个结算顺序，一定要最后结算这个打断，要不打断完了跳转，会继续结算上面两个按键
    ControlDriver::Instance()->unregister_button_callback(reset_auto_start);
    ESP_LOGI(STATEMACHINE,"Out OperatingRecorderState.\n");

}