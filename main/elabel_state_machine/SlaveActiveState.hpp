#ifndef SLAVEACTIVESTATE_HPP
#define SLAVEACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "Esp_now_client.hpp"

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
            set_text_english(ui_SlaveInf, Slave_info);
        }
        else if(get_global_data()->m_language == Chinese)
        {
            char Slave_info[400];
            sprintf(Slave_info, "用户名: %s\n主机Mac: \n " MACSTR "", 
            EspNowSlave::Instance()->username, 
            MAC2STR(EspNowSlave::Instance()->host_mac));
            set_text_chinese(ui_SlaveInf, Slave_info);
        }
    }

    static SlaveActiveState* Instance()
    {
        static SlaveActiveState instance;
        return &instance;
    }
};
#endif