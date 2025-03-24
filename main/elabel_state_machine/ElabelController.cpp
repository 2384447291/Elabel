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
#include "OperatingRecorderState.hpp"
#include "OperatingTimerState.hpp"
ElabelController::ElabelController() : m_elabelFsm(this){}
//初始化状态是init_state

uint8_t ElabelController::get_focus_type()
{
    //当前状态不是focus状态不对
    if(ElabelController::Instance()->m_elabelFsm.GetCurrentState() != FocusTaskState::Instance())
    return 0;
    //如果之前是录音状态，则返回1
    if(ElabelController::Instance()->m_elabelFsm.GetPreviousState() == OperatingRecorderState::Instance())
    {
        return 1;
    }
    //如果之前状态是计时状态，则返回2
    else if(ElabelController::Instance()->m_elabelFsm.GetPreviousState() == OperatingTimeState::Instance())
    {
        return 2;
    }
    //其他状态，则返回3
    else
    {
        return 3;
    }
}

void ElabelController::Init()
{
    m_elabelFsm.Init();
}

void ElabelController::Update()
{
    if(lock_running) return;
    m_elabelFsm.HandleInput();
    m_elabelFsm.Update();
}

void ElabelFsm::HandleInput()
{
    //当没有在任何激活状态下，并且没有激活码，则进入激活状态
    if(GetCurrentState()!=ActiveState::Instance() && GetCurrentState()!=HostActiveState::Instance() && GetCurrentState()!=SlaveActiveState::Instance())
    {
        if(strlen(get_global_data()->m_usertoken) == 0)
        {
            ChangeState(ActiveState::Instance());
        }
    }

    //如果wifi断开连接了，且激活过了，则跳转到激活状态
    if(get_wifi_status() == 0 && strlen(get_global_data()->m_usertoken)!= 0 && GetCurrentState()!=HostActiveState::Instance() && GetCurrentState()!=ActiveState::Instance())
    {
        ESP_LOGI("ElabelFsm","wifi disconnect, reconnect wifi");
        start_blue_activate();
        ChangeState(HostActiveState::Instance());
        m_wifi_connect();
    }


    //-------------------------------整个激活--------------------------------//
    //如果没有被激活，则进入激活状态
    if(GetCurrentState()==ActiveState::Instance())
    {
        if(Is_connect_to_phone())
        {
            get_global_data()->m_is_host = 1;
            set_nvs_info_uint8_t("is_host",get_global_data()->m_is_host);
            ChangeState(HostActiveState::Instance());
        }
        else if(Is_connect_to_host())
        {
            get_global_data()->m_is_host = 2;
            set_nvs_info_uint8_t("is_host",get_global_data()->m_is_host);
            ChangeState(SlaveActiveState::Instance());
        }
    }
    else if(GetCurrentState()==HostActiveState::Instance())
    {
        if(HostActiveState::Instance()->need_forward)
        {
            ChangeState(InitState::Instance());
        }
        else if(HostActiveState::Instance()->need_back)
        {
            ChangeState(ActiveState::Instance());
        }
    }
    //-------------------------------整个激活流程--------------------------------//


    //-------------------------------初始化流程--------------------------------//
    else if(GetCurrentState()==InitState::Instance())
    {
        if(InitState::Instance()->is_need_ota == 1)
        {
            ChangeState(OTAState::Instance());
        }
        else
        {
            //唯一能出去的接口
            if(InitState::Instance()->is_init)
            {
                if(get_global_data()->m_focus_state->is_focus == 1) 
                {
                    ChangeState(FocusTaskState::Instance());
                }
                else ChangeState(ChoosingTaskState::Instance());
            }
        }
    }
    //-------------------------------初始化流程--------------------------------//


    //-------------------------------正常逻辑流程--------------------------------//
    else
    {
        //外部有数据更新打断
        if(get_task_list_state() == firmware_need_update)
        {
            //如果收到了退出focus的信息
            if(get_global_data()->m_focus_state->is_focus == 2)
            {
                ESP_LOGI("ElabelFsm","exit focus");
                if(GetCurrentState()==FocusTaskState::Instance())
                {
                    ChangeState(ChoosingTaskState::Instance());
                }
                get_global_data()->m_focus_state->is_focus = 0;
                get_global_data()->m_focus_state->focus_task_id = 0;
            }
            //如果收到进入focus的信息
            else if(get_global_data()->m_focus_state->is_focus == 1)
            {
                ESP_LOGI("ElabelFsm","enter focus");
                ChangeState(FocusTaskState::Instance());
            }
            //如果只是单纯的更新列表
            else if(get_global_data()->m_focus_state->is_focus == 0)
            {
                ESP_LOGI("ElabelFsm","tasklist update");
                //如果当前是选择task界面则刷新，
                //其他界面则暂时不刷新，反正到选择界面的时候还是会调用一个brush_task_list
                if(GetCurrentState()==ChoosingTaskState::Instance())
                {
                    lock_lvgl();
                    //刷新任务列表
                    ChoosingTaskState::Instance()->brush_task_list();
                    //刷新任务列表
                    ChoosingTaskState::Instance()->recolor_task();
                    //刷新任务列表
                    ChoosingTaskState::Instance()->update_progress_bar();
                    release_lvgl();
                }
            }
            set_task_list_state(newest);
        }

        if(GetCurrentState()==ChoosingTaskState::Instance())
        {
            if(ChoosingTaskState::Instance()->is_jump_to_record_mode)
            {
                ChangeState(OperatingRecorderState::Instance());
            }
            else if(ChoosingTaskState::Instance()->is_jump_to_task_mode)
            {
                ChangeState(OperatingTaskState::Instance());
            }
            else if(ChoosingTaskState::Instance()->is_jump_to_time_mode)
            {
                ChangeState(OperatingTimeState::Instance());
            }
        }
        else if(GetCurrentState()==OperatingRecorderState::Instance())
        {
            //怎么过去怎么回来
            if(OperatingRecorderState::Instance()->need_out_state)
            {
                ChangeState(ElabelController::Instance()->m_elabelFsm.GetPreviousState());
            }
            //如果录音结束，则进入focus状态
            else if(OperatingRecorderState::Instance()->record_process == finish_record_process)
            {
                ChangeState(FocusTaskState::Instance());
            }
        }
        else if(GetCurrentState()==OperatingTaskState::Instance())
        {
            //进入focus状态等着mqtt指令
            if(OperatingTaskState::Instance()->need_out_state)
            {
                ChangeState(ElabelController::Instance()->m_elabelFsm.GetPreviousState());
            }
        }
        else if(GetCurrentState()==OperatingTimeState::Instance())
        {
            if(OperatingTimeState::Instance()->need_out_state)
            {
                //这里不会上一个界面，直接回choose
                ChangeState(ChoosingTaskState::Instance());
            }
            else if(OperatingTimeState::Instance()->need_jump_to_record)
            {
                ChangeState(OperatingRecorderState::Instance());
            }
            else if(OperatingTimeState::Instance()->time_process == finish_time_process)
            {
                ChangeState(FocusTaskState::Instance());
            }
        }
        else if(GetCurrentState()==FocusTaskState::Instance())
        {
            //task状态的focus推出，要看服务器
            if(FocusTaskState::Instance()->need_out_focus && FocusTaskState::Instance()->focus_type != 3)
            {
                ChangeState(ChoosingTaskState::Instance());
            }
        }

        //-------------------------------正常逻辑流程--------------------------------//
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
