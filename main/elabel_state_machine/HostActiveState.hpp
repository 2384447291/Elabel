#ifndef HOSTACTIVESTATE_HPP
#define HOSTACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_host.hpp"

class HostActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool Out_ActiveState = false;

    uint8_t painted_slave_num = 0;

    void repaint_para()
    {
        if(get_global_data()->m_language == English)
        {
            char Host_info[400];
            sprintf(Host_info, "Username: %s\nWIFIname: %s\nWIFIPassword: %s\nSlaveNum: %d", 
            get_global_data()->m_userName, 
            get_global_data()->m_wifi_ssid, 
            get_global_data()->m_wifi_password, 
            EspNowHost::Instance()->slave_num);
            set_text_with_change_font(ui_HostInf, Host_info, false);
        }
        else if(get_global_data()->m_language == Chinese)
        {
            char Host_info[400];
            sprintf(Host_info, "用户名: %s\nWIFI名称: %s\nWIFI密码: %s\n从机数量: %d", 
            get_global_data()->m_userName, 
            get_global_data()->m_wifi_ssid, 
            get_global_data()->m_wifi_password, 
            EspNowHost::Instance()->slave_num);
            set_text_with_change_font(ui_HostInf, Host_info, false);
        }
        painted_slave_num = EspNowHost::Instance()->slave_num;
    }

    static HostActiveState* Instance()
    {
        static HostActiveState instance;
        return &instance;
    }
};
#endif