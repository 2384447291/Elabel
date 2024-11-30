#include "FocusTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "network.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "http.h"

#define INT_TO_STRING(val, str) \
    sprintf(str, "%d", val);

void FocusTaskState::Init(ElabelController* pOwner)
{
    
}

void FocusTaskState::Enter(ElabelController* pOwner)
{
    //ui
    _ui_screen_change(&uic_FocusScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_FocusScreen_screen_init);
    _ui_flag_modify(uic_textlabel7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    // lv_label_set_text(uic_textlabel7, get_global_data()->m_todo_list->items[pOwner->chosenTaskNum].title);
    lv_label_set_text(uic_textlabel7, "fuck");
    _ui_flag_modify(uic_textlabel3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Arc1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    TodoList* todolist = get_global_data()->m_todo_list;
    if(get_wifi_status() == 2 && todolist->size != 0)
    {
        char sstr[12];
        INT_TO_STRING(todolist->items[pOwner->chosenTaskNum].id, sstr)
        printf("title: %s",todolist->items[pOwner->chosenTaskNum].title);
        printf("ENTER :pOwner->chosenTaskNum: %d\n",pOwner->chosenTaskNum);
        http_in_focus(sstr,pOwner->TimeCountdown,false);
    }
    //mainTask_http_state = HTTP_ENTER_FOCUS;
    ESP_LOGI(STATEMACHINE,"Enter FocusTaskState.\n");
}

void FocusTaskState::Execute(ElabelController* pOwner)
{
    if(pOwner->TimeCountdown == 0)
    {
        EventBits_t bits;
        // 等待事件位pdTRUE 表示当 BUTTON_TASK_BIT 被设置时，函数将清除该位。
        //pdFALSE 表示不关心其他位的状态,只有BUTTON_TASK_BIT 被设置时才会被触发
        bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
        if (bits & BUTTON_TASK_BIT) {
            button_interrupt temp_interrupt = get_button_interrupt();
            if(temp_interrupt == short_press)
            {
                pOwner->m_elabelFsm.ChangeState(ChoosingTaskState::Instance());
                //mainTask_http_state = HTTP_OUTFOCUS_DELETE_TODO;
                TodoList* todolist = get_global_data()->m_todo_list;
                if(get_wifi_status() == 2 && todolist->size != 0)
                {
                    char sstr[12];
                    INT_TO_STRING(todolist->items[pOwner->chosenTaskNum].id, sstr)
                    printf("title: %s",todolist->items[pOwner->chosenTaskNum].title);
                    printf("ENTER :pOwner->chosenTaskNum: %d\n",pOwner->chosenTaskNum);
                    http_out_focus(sstr,false);
                }
                return;
            }
        }
    }

    if(pOwner->TimeCountdown > 0 && elabelUpdateTick % 100 == 0)
    {
        pOwner->TimeCountdown--;
        pOwner->needFlashEpaper = true;
        printf("TimeCountdown: %d\n",pOwner->TimeCountdown);
    }

    if(pOwner->needFlashEpaper)
    {
        char timestr[10] = "00:00";
        uint32_t total_seconds = pOwner->TimeCountdown;  // 倒计时总秒数
        uint8_t minutes = total_seconds / 60;  // 计算分钟数
        uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
        // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
        sprintf(timestr, "%02d:%02d", minutes, seconds);
        lv_label_set_text(uic_textlabel3, timestr);
        uint16_t angle_value = ((float)total_seconds / 300.0f) * 30;
        lv_arc_set_bg_angles(ui_Arc1, 0, angle_value);

        pOwner->needFlashEpaper = false;
    }
}

void FocusTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out FocusTaskState.\n");
}