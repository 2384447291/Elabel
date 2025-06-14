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

#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"

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
#include "NoWifiState.hpp"
#include "NoHostState.hpp"
#include "OtaPrepareState.hpp"
#include "InfoState.hpp"
#include "SleepState.hpp"

#define STUCK_TIME 6000
#define STUCK_RELOAD_TIME -4000

void force_reset_elabel()
{
    //如果是主机
    if(get_global_data()->m_is_host == 1)
    {
        http_unbind_device(true, get_global_data()->m_mac_uint);
        for(int i = 0; i < get_global_data()->m_slave_num; i++)
        {
            //通知从机删除自己
            EspNowHost::Instance()->Mqtt_send_unbind_device(get_global_data()->m_slave_info[i].mac);
            //通知后端删除从机
            http_unbind_device(true, get_global_data()->m_slave_info[i].mac);
        }
    }
    //如果是从机
    else if(get_global_data()->m_is_host == 2)
    {
        //通知主机删除自己
        EspNowSlave::Instance()->slave_send_espnow_http_unbind_device();
    }
    reset_elabel();
}

ElabelController::ElabelController() : m_elabelFsm(this) {}
// 初始化状态是init_state

void ElabelController::Init()
{
    m_elabelFsm.Init();
    ChosenTaskId = 0;
    TimeCountdown = (get_global_data()->m_device_info.default_counter_time * 60);
    ChosenTaskNum = 0;
    CenterTaskNum = 0;
    TaskLength = 0;
}

void ElabelController::Update()
{
    m_elabelFsm.HandleInput();
    m_elabelFsm.Update();
}

void ElabelFsm::HandleInput()
{
    //如果在睡眠模式直接返回
    if(get_global_data()->m_is_host == 2)
    {    
        if(GetCurrentState() == SleepState::Instance())
        {
            if(SleepState::Instance()->need_out_state)
            {
                ChangeState(InitState::Instance());
            }
            return;
        }
    }

    // 如果卡死了6s，则重新更新，两个函数都会触发firmware_need_update
    if(m_pOwner->stuck_time > STUCK_TIME)
    {
        ESP_LOGE("ElabelFsm", "stuck need refresh");
        if(get_global_data()->m_is_host == 1)
        {
            //获取任务列表  
            http_get_todo_list(true);
        }
        else if(get_global_data()->m_is_host == 2)
        {
            //获取任务列表  
            EspNowSlave::Instance()->slave_send_espnow_http_get_todo_list();
        }
        m_pOwner->stuck_time = STUCK_RELOAD_TIME;
    }
    
    // 如果没有激活
    if (get_global_data()->m_is_host == 0)
    {
        // 如果当前状态不是激活状态，则进入激活状态
        if (GetCurrentState() != ActiveState::Instance() && GetCurrentState() != HostActiveState::Instance() && GetCurrentState() != SlaveActiveState::Instance())
        {
            // 等待2秒，确保init刷新出来了
            vTaskDelay(pdMS_TO_TICKS(2000));
            ChangeState(ActiveState::Instance());
        }
    }

    // ----------------如果是主机且没有网络，则进入断网状态 ----------------//
    if (get_global_data()->m_is_host == 1)
    {
        if(GetCurrentState() != NoWifiState::Instance())
        {
            if (get_wifi_status() == 0)
            {
                // 等待3秒，确保halfmind刷新出来了,再进入连接模式，开机保护
                vTaskDelay(pdMS_TO_TICKS(3000));
                ChangeState(NoWifiState::Instance());
            }
        }
    }
    
    // 如果是断网状态，则进入初始化状态
    if (GetCurrentState() == NoWifiState::Instance())
    {
        if (NoWifiState::Instance()->need_forward)
        {
            ChangeState(InitState::Instance());
        }
        else if (NoWifiState::Instance()->need_back)
        {
            reset_elabel();
        }
        return;
    }
    // ----------------如果是主机且没有网络，则进入断网状态 ----------------//



    // ----------------如果是从机且长时间没有收到主机消息，则进入断网状态 ----------------//
    if (get_global_data()->m_is_host == 2)
    {
        if(GetCurrentState() != NoHostState::Instance())
        {
            // 如果长时间没有收到主机消息，则进入断网状态
            if (xTaskGetTickCount() - EspNowSlave::Instance()->last_recv_heart_time > pdMS_TO_TICKS(10000))
            {
                ChangeState(NoHostState::Instance());
            }
        }
    }

    // 如果是host，则进入连接模式
    if (GetCurrentState() == NoHostState::Instance())
    {
        if (NoHostState::Instance()->need_forward)
        {
            ChangeState(InitState::Instance());
        }
        else if (NoHostState::Instance()->need_back)
        {
            reset_elabel();
        }
        return;
    }
     // ----------------如果是从机且长时间没有收到主机消息，则进入断网状态 ----------------//



    //-------------------------------整个激活流程--------------------------------//
    // 如果没有被激活，则进入激活状态
    if (GetCurrentState() == ActiveState::Instance())
    {
        if (Is_connect_to_phone())
        {
            ChangeState(HostActiveState::Instance());
        }
        else if (Is_connect_to_host())
        {
            ChangeState(SlaveActiveState::Instance());
        }
    }
    else if (GetCurrentState() == HostActiveState::Instance())
    {
        if (HostActiveState::Instance()->need_back)
        {
            ChangeState(ActiveState::Instance());
        }
    }
    else if (GetCurrentState() == SlaveActiveState::Instance())
    {
        if (SlaveActiveState::Instance()->need_back)
        {
            ChangeState(ActiveState::Instance());
        }
    }
    //-------------------------------整个激活流程--------------------------------//



    //-------------------------------初始化流程--------------------------------//
    else if (GetCurrentState() == InitState::Instance())
    {
        if (InitState::Instance()->need_enter_ota)
        {
            ChangeState(OtaPrepareState::Instance());
        }
        else
        {
            // 唯一能出去的接口
            if (InitState::Instance()->is_init)
            {
                if (get_global_data()->m_focus_state->is_focus == 1)
                {
                    ChangeState(FocusTaskState::Instance());
                }
                else
                {
                    ChangeState(ChoosingTaskState::Instance());
                }
            }
        }
    }
    //-------------------------------初始化流程--------------------------------//

    //-------------------------------OTA流程--------------------------------//
    else if (GetCurrentState() == OtaPrepareState::Instance())
    {
        if (OtaPrepareState::Instance()->need_enter_ota)
        {
            ChangeState(OTAState::Instance());
        }
        else if (OtaPrepareState::Instance()->need_out_ota_prepare)
        {
            //如果此时还没有完成初始化则回初始化
            if(!InitState::Instance()->is_init)
            {
                ChangeState(InitState::Instance());
            }
            else
            {
                esp_restart();
            }
        }
    }
    else if(GetCurrentState() == OTAState::Instance()){}
    //-------------------------------OTA流程--------------------------------//
    
    //这个else排除了激活，初始化和ota共2+1+3=6个状态机
    //-------------------------------正常逻辑流程--------------------------------//
    else
    {
        // 外部数据更新设备打断
        if(get_global_data()->need_update_device_info)
        {
            if(get_global_data()->m_is_host == 1)
            {
                for(int i = 0; i < get_global_data()->m_slave_num; i++)
                {
                    if(get_global_data()->m_slave_info[i].is_sleep)
                    {
                        ESP_LOGI("ElabelFsm", "skip sleep slave "MACSTR" ", MAC2STR(get_global_data()->m_slave_info[i].mac));
                        continue;
                    }
                    else
                    {
                        ESP_LOGI("ElabelFsm", "send device info to slave "MACSTR" ", MAC2STR(get_global_data()->m_slave_info[i].mac));
                        EspNowHost::Instance()->Mqtt_send_device_info(get_global_data()->m_slave_info[i].mac);
                    }
                }
            }
            get_global_data()->need_update_device_info = false;
        }

        //收到这个消息说明在后端已近没有数据了
        if(get_global_data()->need_update_device)
        {
            for(int i = 0; i < get_global_data()->unbind_device_num; i++)
            {
                //如果是主机则要删除所有从机
                if(Same_mac(get_global_data()->m_mac_uint, get_global_data()->unbind_device_mac[i]))
                {
                    http_unbind_device(true, get_global_data()->m_mac_uint);
                    for(int i = 0; i < get_global_data()->m_slave_num; i++)
                    {
                        //通知从机删除自己
                        EspNowHost::Instance()->Mqtt_send_unbind_device(get_global_data()->m_slave_info[i].mac);
                        //通知后端删除从机
                        http_unbind_device(true, get_global_data()->m_slave_info[i].mac);
                    }
                    reset_elabel();  
                }
                //如果是从机则删除他就好了，不用删除后端
                else
                {
                    //主机删除
                    EspNowHost::Instance()->Delete_exist_slave(get_global_data()->unbind_device_mac[i]);
                    //通知从机删除自己,特殊处理了，即使之前给他删除了
                    EspNowHost::Instance()->Mqtt_send_unbind_device(get_global_data()->unbind_device_mac[i]);
                }
            }
            get_global_data()->need_update_device = false;
        }

        // 外部有数据更新打断
        if (get_task_list_state() == firmware_need_update)
        {
            // 如果收到了退出focus的信息
            if (get_global_data()->m_focus_state->is_focus == 2)
            {
                ESP_LOGI("ElabelFsm", "exit focus");
              
                if (GetCurrentState() == FocusTaskState::Instance())
                {
                    ChangeState(ChoosingTaskState::Instance());
                }
                //如果丢包了会有这种情况
                else if(GetCurrentState() == ChoosingTaskState::Instance())
                {
                    lock_lvgl();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->brush_task_list();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->recolor_task();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->update_progress_bar();
                    release_lvgl();
                }
                set_task_list_state(newest);
                // 如果当前是主机，则转发
                if (get_global_data()->m_is_host == 1)
                {
                    EspNowHost::Instance()->Mqtt_out_focus();
                }
            }
            // 如果收到进入focus的信息
            else if (get_global_data()->m_focus_state->is_focus == 1)
            {
                ESP_LOGI("ElabelFsm", "enter focus");
                //如果丢包了会有这种情况
                if (GetCurrentState() == FocusTaskState::Instance())
                {
                    FocusTaskState::Instance()->Exit(m_pOwner);
                    FocusTaskState::Instance()->Enter(m_pOwner);
                }
                else
                {
                    ChangeState(FocusTaskState::Instance());
                }
                set_task_list_state(newest);
                // 如果当前是主机，这段代码要放在下面要不会阻碍刷新
                if (get_global_data()->m_is_host == 1)
                {
                    TodoItem *todo = find_todo_by_id(get_global_data()->m_todo_list, get_global_data()->m_focus_state->focus_task_id);
                    focus_message_t focus_message = pack_focus_message(todo->taskType, todo->fallTiming, todo->startTime, get_global_data()->m_focus_state->focus_task_id, todo->title);
                    //不给地址就是全体转发，告诉所有从机要enterfocus了
                    EspNowHost::Instance()->Mqtt_enter_focus(focus_message);
                }
            }
            // 如果只是单纯的更新列表
            else if (get_global_data()->m_focus_state->is_focus == 0)
            {
                ESP_LOGI("ElabelFsm", "tasklist update");
                if (GetCurrentState() == ChoosingTaskState::Instance())
                {
                    lock_lvgl();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->brush_task_list();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->recolor_task();
                    // 刷新任务列表
                    ChoosingTaskState::Instance()->update_progress_bar();
                    release_lvgl();
                }
                get_global_data()->m_focus_state->is_focus = 0;
                get_global_data()->m_focus_state->focus_task_id = 0;
                set_task_list_state(newest);
                // 如果当前是主机，则转发，这段代码要放在下面要不会阻碍刷新
                if (get_global_data()->m_is_host == 1)
                {
                    //不给地址就是全体转发，告诉所有从机找主机要数据
                    EspNowHost::Instance()->Mqtt_update_task_list();
                }
            }
        }

        if (GetCurrentState() == ChoosingTaskState::Instance())
        {
            if (ChoosingTaskState::Instance()->is_jump_to_record_mode)
            {
                ChangeState(OperatingRecorderState::Instance());
            }
            else if (ChoosingTaskState::Instance()->is_jump_to_task_mode)
            {
                ChangeState(OperatingTaskState::Instance());
            }
            else if (ChoosingTaskState::Instance()->is_jump_to_time_mode)
            {
                ChangeState(OperatingTimeState::Instance());
            }
            else if (ChoosingTaskState::Instance()->is_jump_to_info_mode)
            {
                ChangeState(InfoState::Instance());
            }
            else if (ChoosingTaskState::Instance()->is_jump_to_sleep_mode)
            {
                ChangeState(SleepState::Instance());
            }
        }
        else if (GetCurrentState() == OperatingRecorderState::Instance())
        {
            // 怎么过去怎么回来
            if (OperatingRecorderState::Instance()->need_out_state)
            {
                ChangeState(ElabelController::Instance()->m_elabelFsm.GetPreviousState());
            }
        }
        else if (GetCurrentState() == OperatingTaskState::Instance())
        {
            // 怎么过去怎么回来
            if (OperatingTaskState::Instance()->need_out_state)
            {
                ChangeState(ElabelController::Instance()->m_elabelFsm.GetPreviousState());
            }
        }
        else if (GetCurrentState() == OperatingTimeState::Instance())
        {
            if (OperatingTimeState::Instance()->need_out_state)
            {
                // 这里不会上一个界面，直接回choose
                ChangeState(ChoosingTaskState::Instance());
            }
            else if (OperatingTimeState::Instance()->need_jump_to_record)
            {
                ChangeState(OperatingRecorderState::Instance());
            }
        }
        else if (GetCurrentState() == InfoState::Instance())
        {
            if (InfoState::Instance()->need_back_choose_task)
            {
                ChangeState(ChoosingTaskState::Instance());
            }
            else if (InfoState::Instance()->need_forward_ota)
            {
                ChangeState(OtaPrepareState::Instance());
            }
        }
        //-------------------------------正常逻辑流程--------------------------------//
    }
}

void ElabelFsm::Init()
{
    SetCurrentState(InitState::Instance());
}
