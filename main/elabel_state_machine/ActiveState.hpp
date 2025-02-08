#ifndef ACTIVESTATE_HPP
#define ACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

class ActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    void repaint_para()
    {
        // char Firmwareversion[20];
        // sprintf(Firmwareversion, "Version: %s", FIRMWARE_VERSION);
        // set_text(ui_Firmwareversion, Firmwareversion);

        // char WIFIPassword[40];
        // if(get_global_data()->wifi_password != NULL) sprintf(WIFIPassword, "WIFI Password: %s", get_global_data()->wifi_password);
        // else sprintf(WIFIPassword, "WIFI Password: %s", "Not Found");
        // set_text(ui_WIFIPassword, WIFIPassword);

        // char WIFINAME[40];
        // if(get_global_data()->wifi_ssid != NULL) sprintf(WIFINAME, "WIFI Name: %s", get_global_data()->wifi_ssid);
        // else sprintf(WIFINAME, "WIFI Name: %s", "Not Found");
        // set_text(ui_Wifiname, WIFINAME);

        // char USRNAME[40];
        // if(get_global_data()->userName != NULL) sprintf(USRNAME, "Username: %s", get_global_data()->userName);
        // else sprintf(USRNAME, "Username: %s", "Not Found");
        // set_text(ui_Username, USRNAME);
    }

    static ActiveState* Instance()
    {
        static ActiveState instance;
        return &instance;
    }
};
#endif