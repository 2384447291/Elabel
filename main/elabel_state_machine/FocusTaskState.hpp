#ifndef FOCUSTASKSTATE_HPP
#define FOCUSTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Codec.hpp"
#include "httpmusic.h"
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
    //正向计时
    int inner_time_countup_ms = 0;
    bool need_out_focus = false;
    bool need_enter_sleep = false;
    bool need_flash_paper = false;
    //当前focus的类型1是纯时间，2是task，3是record
    uint8_t focus_type = 0;
    //当前focus的task_id，这个数据是从外部获取的
    int32_t focus_task_id = 0;
    //当前focus的record_message_unique_id,从任务名字中获取，需要和mcodec里的对应
    uint32_t focus_record_message_unique_id = 0;
    //任务描述
    int choose_task_fall_timing = 0;
    long long choose_task_start_time = 0;


    static FocusTaskState* Instance()
    {
        static FocusTaskState instance;
        return &instance;
    }

    void post_music_info()
    {
        //如果是音频任务且音频任务对的上
        if(focus_type == 3 && focus_record_message_unique_id == MCodec::Instance()->record_message_unique_id)
        {
            start_post_music(focus_task_id);
        }
    }

    void get_music_info()
    {
        if(focus_type == 3 && focus_record_message_unique_id != MCodec::Instance()->record_message_unique_id)
        {
            start_get_music(focus_task_id, &MCodec::Instance()->record_message_unique_id, focus_record_message_unique_id);
        }
    }

    void play_focus_music()
    {
        if(focus_type == 3)
        {
            if(focus_record_message_unique_id == MCodec::Instance()->record_message_unique_id)
            {
                MCodec::Instance()->play_mic();
            }
            else
            {
                MCodec::Instance()->play_music("bell");
            }
        }
        else
        {
            MCodec::Instance()->play_music("bell");
        }
    }

    void change_focus_task_label(char* title)
    {
        // 临时字符串缓冲区（原文本的最大长度 + 省略号 + '\0'）
        size_t max_len = strlen(title);
        char buf[max_len + 4];
        strcpy(buf, title);
        uint16_t line_count = 0;

        //1.第一步先判断大字体能不能work单行
        lv_obj_set_height(ui_FocusTask, LV_SIZE_CONTENT);
        lv_label_set_long_mode(ui_FocusTask, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(ui_FocusTask, &ui_font_Chinese32, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_height(ui_Task, LV_SIZE_CONTENT);
        lv_label_set_long_mode(ui_Task, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(ui_Task, &ui_font_Chinese32, LV_PART_MAIN | LV_STATE_DEFAULT); 

        lv_label_set_text(ui_FocusTask, buf);
        lv_obj_update_layout(ui_FocusTask);
        line_count = lv_label_count_lines_wrap(ui_FocusTask,NULL);
        if(line_count <= 1)
        {
            set_text_without_change_font(ui_FocusTask,buf);
            ESP_LOGI("focus","大字体单行放得下");
            return;
        }

        //2.第二步判断2行的小字体放不放的下
        lv_obj_set_height(ui_FocusTask, LV_SIZE_CONTENT);
        lv_label_set_long_mode(ui_FocusTask, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(ui_FocusTask, &ui_font_Chinese24, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_height(ui_Task, LV_SIZE_CONTENT);
        lv_label_set_long_mode(ui_Task, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(ui_Task, &ui_font_Chinese24, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_label_set_text(ui_FocusTask, buf);
        lv_obj_update_layout(ui_FocusTask);
        memset(buf, 0, sizeof(buf));
        line_count = lv_label_count_lines_wrap(ui_FocusTask,buf); 
        if(line_count <= 2)
        {
            set_text_without_change_font(ui_FocusTask,buf);
            ESP_LOGI("focus","小字体两行放得下");
            return;
        }

        //3.第三步如果2行放不下，则根据主从机分开判断
        //如果是主机
        if(get_global_data()->m_is_host == 1)
        {
            lv_obj_set_height(ui_FocusTask, 40);
            lv_label_set_long_mode(ui_FocusTask, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_text_font(ui_FocusTask, &ui_font_Chinese32, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_height(ui_Task, 40);
            lv_label_set_long_mode(ui_Task, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_text_font(ui_Task, &ui_font_Chinese32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_update_layout(ui_FocusTask);
            set_text_without_change_font(ui_FocusTask,title);
            ESP_LOGI("focus","主机滚动字体");
        }
        //如果是从机
        else if(get_global_data()->m_is_host == 2)
        {
            // 从末尾开始替换字符
            max_len = strlen(buf);
            for (int len = max_len - 1; len > 0; --len) {
                // 保证裁剪不会破坏 UTF-8 字符
                while ((buf[len] & 0xC0) == 0x80) len--;  // 跳过 UTF-8 尾字节
                strcpy(&buf[len],"...");  // 用省略号替代

                lv_label_set_text(ui_FocusTask, buf);
                lv_obj_update_layout(ui_FocusTask);
                if (lv_label_count_lines_wrap(ui_FocusTask,NULL) <= 2) {
                    set_text_without_change_font(ui_FocusTask, buf);
                    break;
                }
            }
            ESP_LOGI("focus","从机省略号字体");
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