#ifndef SLAVEACTIVESTATE_HPP
#define SLAVEACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_slave.hpp"
#include "Esp_now_client.hpp"

#define Too_close_score 400
#define Good_score 200
#define Bad_score 50

typedef enum
{
    default_Slaveactive_process,
    Slaveactive_waiting_connect_process,
    Slaveactive_test_connect_process,
    Slaveactive_bind_host_process,
    Slaveactive_success_connect_process,
} Slave_Active_process;


typedef enum
{
    default_connect_state,
    Too_close,
    Good,
    Bad,
    Lose_connect,
}Slave_connect_State;

class SlaveActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    Slave_Active_process slave_active_process = default_Slaveactive_process;
    Slave_connect_State slave_connect_state = default_connect_state;

    bool button_slave_active_confirm_left = false;
    bool need_back = false;
    bool need_flash_paper = false;
    int16_t score = 0;

    void set_connect_state(Slave_connect_State state)
    {
        switch(state)
        {
            case Too_close:
                lv_obj_clear_flag(ui_SlaveActivateTooClose, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateGood, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateBad, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateLoseConnection, LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_flag(ui_SlaveActivateRetry, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateAccept, LV_OBJ_FLAG_HIDDEN);
                break;
            case Good:
                lv_obj_add_flag(ui_SlaveActivateTooClose, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateGood, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateBad, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateLoseConnection, LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_flag(ui_SlaveActivateAccept, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateRetry, LV_OBJ_FLAG_HIDDEN);
                break;
            case Bad:
                lv_obj_add_flag(ui_SlaveActivateTooClose, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateGood, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateBad, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateLoseConnection, LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_flag(ui_SlaveActivateAccept, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateRetry, LV_OBJ_FLAG_HIDDEN);
                break;
            case Lose_connect:
                lv_obj_add_flag(ui_SlaveActivateTooClose, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateGood, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateBad, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_SlaveActivateLoseConnection, LV_OBJ_FLAG_HIDDEN);

                lv_obj_clear_flag(ui_SlaveActivateAccept, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_SlaveActivateRetry, LV_OBJ_FLAG_HIDDEN);
                break;
            case default_connect_state:
                break;
            default:
                break;
        }
    }

    void update_score(int16_t score)
    {
        if(score > Too_close_score)
        {
            if(slave_connect_state != Too_close)
            {
                slave_connect_state = Too_close;
                set_connect_state(slave_connect_state);
            }
        }
        else if(score > Good_score)
        {
            if(slave_connect_state != Good)
            {
                slave_connect_state = Good;
                set_connect_state(slave_connect_state);
            }
        }
        else if(score > Bad_score)
        {
            if(slave_connect_state != Bad)
            {
                slave_connect_state = Bad;
                set_connect_state(slave_connect_state);
            }
        }
        else
        {
            if(slave_connect_state != Lose_connect)
            {
                slave_connect_state = Lose_connect;
                set_connect_state(slave_connect_state);
            }
        }
    }

    void enter_test_connect()
    {
        slave_active_process = Slaveactive_test_connect_process;
        EspNowClient::Instance()->start_test_connecting_task();
        lock_lvgl();
        switch_screen(ui_SlaveActiveScreen);
        lv_obj_add_flag(ui_ConnectingHost, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TestConnecting, LV_OBJ_FLAG_HIDDEN);   
        slave_connect_state = Too_close;
        set_connect_state(slave_connect_state); 
        release_lvgl();
        need_flash_paper = false;
    }

    void enter_connect_host()
    {
        slave_active_process = Slaveactive_waiting_connect_process;
        lock_lvgl();
        switch_screen(ui_SlaveActiveScreen);
        lv_obj_clear_flag(ui_ConnectingHost, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_TestConnecting, LV_OBJ_FLAG_HIDDEN);  
        Global_data* global_data = get_global_data();
        set_text_without_change_font(ui_Username, global_data->m_userName);
        button_slave_active_confirm_left = false;
        lv_obj_clear_state(ui_SlaveActiveCancel, LV_STATE_PRESSED );
        lv_obj_add_state(ui_SlaveActiveConfirm, LV_STATE_PRESSED );
        release_lvgl();
    }

    static SlaveActiveState* Instance()
    {
        static SlaveActiveState instance;
        return &instance;
    }
};
#endif