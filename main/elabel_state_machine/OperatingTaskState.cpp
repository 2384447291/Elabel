#include "OperatingTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"

void OperatingTaskState::Init(ElabelController* pOwner)
{
    
}

void OperatingTaskState::Enter(ElabelController* pOwner)
{
    pOwner->TimeCountdown = pOwner->TimeCountdownOffset = 300;
    lv_arc_set_bg_angles(ui_Arc1, 0, 30);
    // ui
    _ui_screen_change(&uic_FocusScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_FocusScreen_screen_init);
    _ui_flag_modify(uic_textlabel7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

    _ui_flag_modify(uic_textlabel3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Arc1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    pOwner->needFlashEpaper = true;
    enterOperatingTime = elabelUpdateTick;
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.\n");
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
    EventBits_t bits;
    // 等待事件位pdTRUE 表示当 BUTTON_TASK_BIT 被设置时，函数将清除该位。
    //pdFALSE 表示不关心其他位的状态,只有BUTTON_TASK_BIT 被设置时才会被触发
    bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
    if (bits & ENCODER_TASK_BIT) {
        encoder_interrupt temp_interrupt = get_encoder_interrupt();
        if(temp_interrupt == right_circle)
        {
            pOwner->TimeCountdown += 300;
            pOwner->TimeCountdownOffset = pOwner->TimeCountdown;
        }
        else if(temp_interrupt == left_circle)
        {
            if(pOwner->TimeCountdown <= 300) pOwner->TimeCountdown = 0;            
            else pOwner->TimeCountdown -= 300;
            pOwner->TimeCountdownOffset = pOwner->TimeCountdown;
        }
        pOwner->needFlashEpaper = true;
        enterOperatingTime = elabelUpdateTick;
    }
    else
    {
        if(elabelUpdateTick - enterOperatingTime > 500)//5s
        {
            pOwner->m_elabelFsm.ChangeState(FocusTaskState::Instance());
            return;
        }
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

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}