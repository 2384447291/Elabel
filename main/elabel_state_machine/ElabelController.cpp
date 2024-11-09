#include "ElabelController.hpp"
#include "global_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "control_driver.h"

#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"

ElabelController::ElabelController() : m_elabelFsm(this){}

void ElabelController::Init()
{
    m_elabelFsm.Init();
}

void ElabelController::Update()
{
    m_elabelFsm.HandleInput();
    m_elabelFsm.Update();
}

void ElabelFsm::HandleInput()
{
    EventBits_t bits;
    // 等待事件位pdTRUE 表示当 BUTTON_TASK_BIT 被设置时，函数将清除该位。
    //pdFALSE 表示不关心其他位的状态,只有BUTTON_TASK_BIT 被设置时才会被触发
    bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
    if (bits & BUTTON_TASK_BIT) {
        button_interrupt temp_interrupt = get_button_interrupt();
        if(temp_interrupt == short_press)
        {
            ChangeState(ChoosingTaskState::Instance());
        }
        else if(temp_interrupt == long_press)
        {
            ChangeState(OperatingTaskState::Instance());
        }
    }
}

void ElabelFsm::Init()
{
    ChoosingTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    SetCurrentState(ChoosingTaskState::Instance());
}

