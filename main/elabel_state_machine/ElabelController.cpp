#include "ElabelController.hpp"
#include "global_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "control_driver.h"
#include "ssd1680.h"
#include "network.h"

#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "NoWifiState.hpp"
#include "InitState.hpp"

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
    //wifi事件打断
    static bool led_wifi_on;
    if(get_wifi_status() == 0)
    {
        ChangeState(NoWifiState::Instance());
    }
    else if(get_wifi_status() == 1)
    {
        led_wifi_on = true;
        set_led_state(device_wifi_connect);
    }
    else if(get_wifi_status() == 2 && led_wifi_on)
    {
        set_led_state(no_light);
        led_wifi_on = false;
    }

    // 外部中断打断
    if(get_task_list_state() == firmware_need_update)
    {
        //如果收到了退出focus的信息
        if(get_global_data()->m_focus_state->is_focus == 2)
        {
            if(GetCurrentState()==FocusTaskState::Instance())
            {
                ChoosingTaskState::Instance()->is_confirm_task = false;
                ChangeState(ChoosingTaskState::Instance());
            }
            get_global_data()->m_focus_state->is_focus = 0;
            get_global_data()->m_focus_state->focus_task_id =0;
        }
        //如果收到进入focus的信息
        else if(get_global_data()->m_focus_state->is_focus == 1)
        {
            if(GetCurrentState()!=InitState::Instance() 
            && GetCurrentState()!=NoWifiState::Instance())
            {
                ChangeState(FocusTaskState::Instance());
            }
        }
        else if(get_global_data()->m_focus_state->is_focus == 0)
        {
            //如果当前是选择task界面则刷新其他界面则暂时不刷新
            if(GetCurrentState()==ChoosingTaskState::Instance())
            {
                ChoosingTaskState::Instance()->need_stay_choosen = false;
                lock_lvgl();
                ChoosingTaskState::Instance()->brush_task_list();
                release_lvgl();
                ElabelController::Instance()->need_flash_paper = true;
            }
        }
        set_task_list_state(newest);
    }

    //正常流程跳转
    if(GetCurrentState()==NoWifiState::Instance())
    {
        if(get_wifi_status() == 2)
        {
            ChangeState(InitState::Instance());
        }
    }

    if(GetCurrentState()==InitState::Instance())
    {
        //init模式肯定刷新过任务列表
        if(InitState::Instance()->is_init)
        {
            //如果初始完成后有task是专注模式
            if(get_global_data()->m_focus_state->is_focus == 1)
            {
                ChangeState(FocusTaskState::Instance());
            }
            //如果初始完成后没有task是专注模式
            else
            {
                ChoosingTaskState::Instance()->need_stay_choosen = false;
                ChangeState(ChoosingTaskState::Instance());
            }
        }
    }
    
    if(GetCurrentState()==ChoosingTaskState::Instance())
    {
        if(ChoosingTaskState::Instance()->is_confirm_task)
        {
            ChangeState(OperatingTaskState::Instance());
            ChoosingTaskState::Instance()->is_confirm_task = false;
        }
    }

    if(GetCurrentState()==OperatingTaskState::Instance())
    {
        if(OperatingTaskState::Instance()->is_not_confirm_task)
        {
            ChoosingTaskState::Instance()->need_stay_choosen = true;
            ChangeState(ChoosingTaskState::Instance());
        }
        //交给服务器传递
        // else if(OperatingTaskState::Instance()->is_confirm_time)
        // {
        //     ChangeState(FocusTaskState::Instance());
        // }
    }

    //交给服务器传递
    // if(GetCurrentState()==FocusTaskState::Instance())
    // {
    //     if(FocusTaskState::Instance()->is_out_focus)
    //     {
    //         ChoosingTaskState::Instance()->need_stay_choosen = true;
    //         ChangeState(ChoosingTaskState::Instance());
    //     }
    // }

}

void ElabelFsm::Init()
{
    ChoosingTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    FocusTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    InitState::Instance()->Init(m_pOwner);
    SetCurrentState(InitState::Instance());
}
