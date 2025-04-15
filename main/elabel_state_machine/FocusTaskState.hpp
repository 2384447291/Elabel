#ifndef FOCUSTASKSTATE_HPP
#define FOCUSTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Codec.hpp"
class FocusTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    int inner_time_countdown_ms = 0;
    int inner_time_countdown_s = 0;
    bool need_out_focus = false;
    uint8_t focus_type = 0;
    bool need_flash_paper = false;

    static FocusTaskState* Instance()
    {
        static FocusTaskState instance;
        return &instance;
    }

    void play_focus_music()
    {
        if(false)
        {
            MCodec::Instance()->play_mic();
        }
        else
        {
            MCodec::Instance()->play_music("bell");
        }
    }

    void flush_focus_time()
    {
        char timestr[15] = "00:00";
        uint8_t minutes = 0;
        uint8_t seconds = 0;
        uint32_t total_seconds = 0;
        if(inner_time_countdown_s <= 0)
        {
            if(focus_type == 1)
            {
                lv_obj_clear_flag(ui_NoFocusWarning, LV_OBJ_FLAG_HIDDEN);
            }
            else if(focus_type == 2 || focus_type == 0)
            {
                lv_obj_clear_flag(ui_TaskFocusWarning, LV_OBJ_FLAG_HIDDEN);
            }
            else if(focus_type == 3)
            {
                lv_obj_clear_flag(ui_RecordFocusWarning, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if(inner_time_countdown_s >=0)
        {
            total_seconds = inner_time_countdown_s;   // 倒计时总秒数
            minutes = total_seconds / 60;  // 计算分钟数
            seconds = total_seconds % 60;  // 计算剩余秒数
            // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
            sprintf(timestr, "%02d:%02d", minutes, seconds);
        }
        else
        {
            total_seconds = -inner_time_countdown_s;   // 倒计时总秒数
            minutes = total_seconds / 60;  // 计算分钟数
            seconds = total_seconds % 60;  // 计算剩余秒数
            // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
            sprintf(timestr, "+%02d:%02d", minutes, seconds);
        }

        if(focus_type == 1)
        {
            set_text_without_change_font(ui_NoFocusTime, timestr);
        }
        else if(focus_type == 2 || focus_type == 0)
        {
            set_text_without_change_font(ui_TaskFocusTime, timestr);
        }
        else if(focus_type == 3)
        {
            set_text_without_change_font(ui_RecordFocusTime, timestr); 
        }
    }
};

#endif