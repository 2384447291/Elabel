#ifndef HOSTACTIVESTATE_HPP
#define HOSTACTIVESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "esp_now_host.hpp"
#define RECONNECT_COUNT_DOWN 30

class HostActiveState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool button_choose_left = true;
    bool is_fail = false;
    bool is_set_wifi = false;

    bool need_back = false;
    bool need_forward = false;

    uint32_t reconnect_count_down = RECONNECT_COUNT_DOWN;

    static HostActiveState* Instance()
    {
        static HostActiveState instance;
        return &instance;
    }
};
#endif