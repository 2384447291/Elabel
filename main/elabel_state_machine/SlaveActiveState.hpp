#ifndef SLAVEACTIVESTATE_HPP
#define SLAVEACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_slave.hpp"

class SlaveActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool Out_ActiveState = false;

    void repaint_para()
    {
        if(get_global_data()->m_language == English)
        {
            char Slave_info[400];
            sprintf(Slave_info, "Username: %s\nHostMac: \n " MACSTR "", 
            EspNowSlave::Instance()->username, 
            MAC2STR(EspNowSlave::Instance()->host_mac));
            set_text_with_change_font(ui_SlaveInf, Slave_info, false);
        }
        else if(get_global_data()->m_language == Chinese)
        {
            char Slave_info[400];
            sprintf(Slave_info, "用户名: %s\n主机Mac: \n " MACSTR "", 
            EspNowSlave::Instance()->username, 
            MAC2STR(EspNowSlave::Instance()->host_mac));
            set_text_with_change_font(ui_SlaveInf, Slave_info, false);
        }
    }

    static SlaveActiveState* Instance()
    {
        static SlaveActiveState instance;
        return &instance;
    }
};
#endif