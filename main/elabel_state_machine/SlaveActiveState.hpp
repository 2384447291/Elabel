#ifndef SLAVEACTIVESTATE_HPP
#define SLAVEACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_slave.hpp"
#include "Esp_now_client.hpp"

typedef enum
{
    default_Slaveactive_process,
    Slaveactive_waiting_connect_process,
    Slaveactive_test_connect_process,
    Slaveactive_success_connect_process,
} Slave_Active_process;

typedef enum
{
    default_test_connect_process,
    test_connect_process_start,
    test_connect_process_send_packet,
    test_connect_process_stop,
    test_waiting_ack_process,
}Test_connect_process;

class SlaveActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    Slave_Active_process slave_active_process = default_Slaveactive_process;

    bool button_slave_active_confirm_left = false;
    bool need_back = false;
    bool need_forward = false;
    bool need_flash_paper = false;

    void start_test_connecting_task();
    void stop_test_connecting_task();
    Test_connect_process test_connect_process = default_test_connect_process;

    void enter_test_connect()
    {
        slave_active_process = Slaveactive_test_connect_process;
        lock_lvgl();
        switch_screen(ui_SlaveActiveScreen);
        lv_obj_add_flag(ui_ConnectingHost, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_TestConnecting, LV_OBJ_FLAG_HIDDEN);   
        set_text_without_change_font(ui_ConnectGuide2, "Score: 100");
        release_lvgl();
        test_connect_process = test_connect_process_start;
        start_test_connecting_task();
        need_flash_paper = false;
<<<<<<< Updated upstream
=======
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
>>>>>>> Stashed changes
    }

    static SlaveActiveState* Instance()
    {
        static SlaveActiveState instance;
        return &instance;
    }
};
#endif