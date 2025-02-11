#include "ElabelController.hpp"
#include "global_message.h"
#include "global_draw.h"
#include "global_time.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "control_driver.hpp"
#include "ssd1680.h"
#include "network.h"

#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "ActiveState.hpp"
#include "InitState.hpp"
#include "OTAState.hpp"
#include "HostActiveState.hpp"
#include "SlaveActiveState.hpp"
ElabelController::ElabelController() : m_elabelFsm(this){}
//初始化状态是init_state

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
    if(GetCurrentState()==InitState::Instance())
    {
        if(InitState::Instance()->is_need_ota)
        {
            ChangeState(OTAState::Instance());
        }
        else
        {
            //当没有用户或者没有网络的时候要进入绑定状态，如果有历史消息肯定在进入主程序前就已经开始wifi连接了
            if(get_wifi_status() == 0 || strlen(get_global_data()->m_usertoken) == 0)
            {
                ChangeState(ActiveState::Instance());
            }
            //唯一能出去的接口
            if(InitState::Instance()->is_init)
            {
                if(get_global_data()->m_focus_state->is_focus == 1) 
                {
                    ElabelController::Instance()->focus_by_myself = false;
                    ChangeState(FocusTaskState::Instance());
                }
                else ChangeState(ChoosingTaskState::Instance());
            }
        }
    }
    else if(GetCurrentState()==ActiveState::Instance())
    {
        if(Is_connect_to_phone())
        {
            ChangeState(HostActiveState::Instance());
        }
        else if(Is_connect_to_host())
        {
            ChangeState(SlaveActiveState::Instance());
        }
    }
    else if(GetCurrentState()==HostActiveState::Instance())
    {
        if(HostActiveState::Instance()->Out_ActiveState)
        {
            ChangeState(InitState::Instance());
        }
    }
    else
    {

        //外部有数据更新打断
        if(get_task_list_state() == firmware_need_update)
        {
            //如果收到了退出focus的信息
            if(get_global_data()->m_focus_state->is_focus == 2)
            {
                ESP_LOGI("ElabelFsmOuter","exit focus");
                if(GetCurrentState()==FocusTaskState::Instance())
                {
                    ChoosingTaskState::Instance()->need_stay_choosen = false;
                    ChangeState(ChoosingTaskState::Instance());
                }
                get_global_data()->m_focus_state->is_focus = 0;
                get_global_data()->m_focus_state->focus_task_id = 0;
            }
            //如果收到进入focus的信息
            else if(get_global_data()->m_focus_state->is_focus == 1)
            {
                ESP_LOGI("ElabelFsmOuter","enter focus");
                ElabelController::Instance()->focus_by_myself = false;
                ChangeState(FocusTaskState::Instance());
            }
            else if(get_global_data()->m_focus_state->is_focus == 0)
            {
                ESP_LOGI("ElabelFsmOuter","tasklist update");
                //如果当前是选择task界面则刷新其他界面则暂时不刷新，反正到选择界面的时候还是会调用一个brush_task_list
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

        if(GetCurrentState()==ChoosingTaskState::Instance())
        {
            if(ChoosingTaskState::Instance()->is_jump_to_activate)
            {
                ChangeState(ActiveState::Instance());
            }
            else if(ChoosingTaskState::Instance()->is_confirm_task)
            {
                ChangeState(OperatingTaskState::Instance());
            }
        }
        else if(GetCurrentState()==OperatingTaskState::Instance())
        {
            if(OperatingTaskState::Instance()->is_not_confirm_task)
            {
                ChoosingTaskState::Instance()->need_stay_choosen = true;
                ChangeState(ChoosingTaskState::Instance());
            }
        }
    }
}

void ElabelFsm::Init()
{
    ChoosingTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    FocusTaskState::Instance()->Init(m_pOwner);
    OperatingTaskState::Instance()->Init(m_pOwner);
    InitState::Instance()->Init(m_pOwner);
    OTAState::Instance()->Init(m_pOwner);
    SetCurrentState(InitState::Instance());
}
