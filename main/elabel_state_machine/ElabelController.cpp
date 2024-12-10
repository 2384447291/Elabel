#include "ElabelController.hpp"
#include "global_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "control_driver.h"
#include "ssd1680.h"

#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"

ElabelController::ElabelController() : m_elabelFsm(this){}

void ElabelController::Init()
{
    tasklen = 0;
    last_tasklen = 0;
    task_list = NULL;
    TimeCountdownOffset = TimeCountdown = 300;
    m_elabelFsm.Init();
}

void ElabelController::Update()
{
    if(stop_mainTask)
    {
        //
        return;
    }
    APP_task_state task_state = APP_TASK();
    if(APP_FocusState_update() || task_state != TASK_NO_CHANGE)//收APP消息
    {
        set_task_list_state(newest);//置标为newest
        needFlashEpaper = true;
        printf("APP_TASK: %d\n",task_state);
        return;
    }

    if(needFlashEpaper)
    {
        flash_tick = elabelUpdateTick;
        entersleep = false;
    }
    else if(elabelUpdateTick - flash_tick > 3000 && !entersleep)
    {
        ssd1680_deep_sleep();
        entersleep = true;
    }

    m_elabelFsm.HandleInput();
    // if(elabelUpdateTick == 499)
    // {
    //     TimeCountdown = 300;
    //     TimeCountdownOffset = 300;
    //     chosenTaskNum = 0;
    // }
    m_elabelFsm.Update();
}

void ElabelFsm::HandleInput()
{
    // EventBits_t bits;
    // // 等待事件位pdTRUE 表示当 BUTTON_TASK_BIT 被设置时，函数将清除该位。
    // //pdFALSE 表示不关心其他位的状态,只有BUTTON_TASK_BIT 被设置时才会被触发
    // bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
    // if (bits & BUTTON_TASK_BIT) {
    //     button_interrupt temp_interrupt = get_button_interrupt();
    //     if(temp_interrupt == short_press)
    //     {
    //         pOwner->m_elabelFsm.ChangeState(ChoosingTaskState::Instance());
    //     }
    //     else if(temp_interrupt == long_press)
    //     {
    //         pOwner->m_elabelFsm.ChangeState(OperatingTaskState::Instance());
    //     }
    // }

    // if(elabelUpdateTick == 500)
    // {
    //     _ui_screen_change(&ui_HalfmindScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_HalfmindScreen_screen_init);
    //     ElabelStateSet(HALFMIND_STATE);
    // }

}

void ElabelFsm::Init()
{
    ChoosingTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    FocusTaskState::Instance()->Init(m_pOwner);
    SetCurrentState(ChoosingTaskState::Instance());
}

bool ElabelController::APP_FocusState_update()
{
    if(get_task_list_state() != firmware_need_update) return false;
    
    bool isFocus = (bool)(get_global_data()->m_focus_state->is_focus - 2);
    int FocusId = get_global_data()->m_focus_state->focus_task_id; 

    if(m_elabelFsm.GetCurrentState() == FocusTaskState::Instance() && !isFocus)
    {
        m_elabelFsm.ChangeState(ChoosingTaskState::Instance());
        return true;
    }
    else if(m_elabelFsm.GetCurrentState() != FocusTaskState::Instance() && isFocus)
    {
        TodoList* todolist = get_global_data()->m_todo_list;
        TodoItem* Focusitem = find_todo_by_id(todolist,FocusId);

        if(Focusitem == NULL)
        {
            printf("Focusitem is NULL\n");
            return false;
        }

        TimeCountdownOffset = TimeCountdown = Focusitem->fallTiming;
        chosenTaskNum = get_task_position(task_list,Focusitem->title);
        m_elabelFsm.ChangeState(FocusTaskState::Instance());
        return true;
    }
    else return false;
}

APP_task_state ElabelController::APP_TASK(void)
{
    if(get_task_list_state() != firmware_need_update) return TASK_NO_CHANGE;
    else printf("task_list_state is firmware_need_update\n");
    
    TodoList* todolist = get_global_data()->m_todo_list;
    if(first_time_to_sync_tasklist)
    {
        int temp = tasklen;
        for(int i = 0; i < temp;i++)
        {
            char *task_title = find_task_by_position(task_list, i);
            if(task_title == NULL) continue;
            TodoItem* item = find_todo_by_title(todolist, task_title);
            if(item == NULL)
            {
                delete_task(&task_list, i);
                i--;
            }
        }
        for(int i = tasklen;i < todolist->size;i++)
        {
            add_task(&task_list, todolist->items[i].title);
        }
        tasklen = todolist->size;
        return ADD_TASK;
    }

    if(todolist->size > tasklen)
    {
        for(int i = tasklen;i < todolist->size;i++)
        {
            add_task(&task_list, todolist->items[i].title);
        }
        tasklen = todolist->size;
        return ADD_TASK;
    }
    else if(todolist->size < tasklen)
    {
        int temp = tasklen;
        for(int i = 0; i < temp;i++)
        {
            char *task_title = find_task_by_position(task_list, i);
            if(task_title == NULL) continue;
            TodoItem* item = find_todo_by_title(todolist, task_title);
            if(item == NULL)
            {
                delete_task(&task_list, i);
                i--;
            }
        }
        printf("tasklen: %d\n",tasklen);
        return DELETE_TASK;
    }
    else
    {
        for(int i = 0; i < tasklen;i++)
        {
            TodoItem* item = find_todo_by_title(todolist, find_task_by_position(task_list, i));
            if(item == NULL)
            {
                modify_task(task_list, i, todolist->items[i].title);
            }
        }
        return UPDATE_TASK;
    }
}

