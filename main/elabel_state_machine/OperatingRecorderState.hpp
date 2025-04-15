#ifndef OPERATINGRECORDERSTATE_HPP
#define OPERATINGRECORDERSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "codec.hpp"
#include "Esp_now_slave.hpp"
#include "http.h"
#define RECORD_TIME 5
#define CONFIRM_VOICE_TIME 15
#define RECONFIRM_VOICE_TIME 3

typedef enum
{
    default_record_process,
    Record_voice_process,
    Record_time_process,
    Record_reconfirm_process,
    finish_record_process,
} Record_process;


class OperatingRecorderState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static OperatingRecorderState* Instance()
    {
        static OperatingRecorderState instance;
        return &instance;
    }

    Record_process record_process = default_record_process;
    bool button_choose_record_confirm_left = false;
    uint8_t record_voice_countdown = RECORD_TIME;
    uint8_t confirm_voice_countdown = CONFIRM_VOICE_TIME;
    uint8_t reconfirm_process_countdown = RECONFIRM_VOICE_TIME;
    bool need_flash_paper = false;
    bool need_out_state = false;

    void enter_screen_record_voice()
    {
        ESP_LOGI("OperatingRecorderState", "enter_screen_record_voice");
        record_process = Record_voice_process;
        record_voice_countdown = RECORD_TIME;
        MCodec::Instance()->start_record();

        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_clear_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);
        //上层文字显示
        lv_obj_clear_flag(ui_RecordOperateUpText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordOperateTime, LV_OBJ_FLAG_HIDDEN);   

        //中间文字显示
        lv_obj_clear_flag(ui_RecordOperateMiddleText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordAgain, LV_OBJ_FLAG_HIDDEN);

        //下层文字显示
        lv_obj_clear_flag(ui_RecordOperateButton, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordComfirmButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(ui_RecordComfirmCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_RecordComfirmFinish, LV_STATE_PRESSED );

        lv_obj_add_flag(ui_RecordOperateDownText, LV_OBJ_FLAG_HIDDEN);

        char time_str[20];
        sprintf(time_str, "%ds left", record_voice_countdown);
        set_text_without_change_font(ui_RecordOperateMiddleText, time_str);

        release_lvgl();
    }

    void enter_screen_confirm_voice()
    {
        ESP_LOGI("OperatingRecorderState", "enter_screen_confirm_voice");
        record_process = Record_time_process;
        need_flash_paper = true;
        MCodec::Instance()->stop_record();
        confirm_voice_countdown = CONFIRM_VOICE_TIME;
        button_choose_record_confirm_left = false;
        MCodec::Instance()->play_mic();

        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_clear_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);
        //上层文字显示
        lv_obj_add_flag(ui_RecordOperateUpText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_RecordOperateTime, LV_OBJ_FLAG_HIDDEN);   

        //中间文字显示
        lv_obj_add_flag(ui_RecordOperateMiddleText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_RecordAgain, LV_OBJ_FLAG_HIDDEN);

        //下层文字显示
        lv_obj_add_flag(ui_RecordOperateButton, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_RecordComfirmButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_RecordComfirmCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_RecordComfirmFinish, LV_STATE_PRESSED );

        lv_obj_add_flag(ui_RecordOperateDownText, LV_OBJ_FLAG_HIDDEN);

        //设置设定的时间
        char timestr[10] = "00:00";
        uint32_t total_seconds = ElabelController::Instance()->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        set_text_without_change_font(ui_RecordOperateTime, timestr);

        release_lvgl();
    }

    void enter_screen_reconfirm_process() 
    {
        ESP_LOGI("OperatingRecorderState", "enter_screen_reconfirm_process");
        record_process = Record_reconfirm_process;
        reconfirm_process_countdown = RECONFIRM_VOICE_TIME;
        lock_lvgl();
        switch_screen(ui_OperatingScreen);
        lv_obj_clear_flag(ui_RecordOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TaskOperate, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_NoOperate, LV_OBJ_FLAG_HIDDEN);
        //上层文字显示
        lv_obj_add_flag(ui_RecordOperateUpText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_RecordOperateTime, LV_OBJ_FLAG_HIDDEN);   

        //中间文字显示
        lv_obj_add_flag(ui_RecordOperateMiddleText, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_RecordAgain, LV_OBJ_FLAG_HIDDEN);

        //下层文字显示
        lv_obj_add_flag(ui_RecordOperateButton, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_RecordComfirmButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(ui_RecordComfirmCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_RecordComfirmFinish, LV_STATE_PRESSED );

        lv_obj_clear_flag(ui_RecordOperateDownText, LV_OBJ_FLAG_HIDDEN);

        char time_str[30];
        sprintf(time_str, "Auto start in %d secs", reconfirm_process_countdown);
        set_text_without_change_font(ui_RecordOperateDownText, time_str);

        release_lvgl();
    }

    void enter_screen_finish_record()
    {
        ESP_LOGI("OperatingRecorderState", "enter_screen_finish_record");
        record_process = finish_record_process;
        if(get_global_data()->m_is_host == 1)
        {
            http_add_to_do((char*)"Record Task",(char*)"3",true);
            bool is_add_task = false;
            int todo_id = 0;
            do
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                for(int i = 0; i < get_global_data()->m_todo_list->size; i++)
                {
                    if(get_global_data()->m_todo_list->items[i].taskType == 3)
                    {
                        is_add_task = true;
                        todo_id = get_global_data()->m_todo_list->items[i].id;
                    }
                }
            } while (!is_add_task && get_task_list_state() == newest);
            char sstr[12];
            sprintf(sstr, "%d", todo_id);
            http_in_focus(sstr,ElabelController::Instance()->TimeCountdown,false);
        }
        else if(get_global_data()->m_is_host == 2)
        {
            char title[20] = "Record Task";
            focus_message_t focus_message = pack_focus_message(3, ElabelController::Instance()->TimeCountdown, 0, title);
            EspNowSlave::Instance()->slave_send_espnow_http_enter_focus_task(focus_message);
        }
    }
};
#endif