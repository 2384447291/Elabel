#ifndef OPERATINGTASKSTATE_HPP
#define OPERATINGTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

#define auto_enter_time 1000; //1000次20ms，刚好20s

class OperatingTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool is_not_confirm_task = false;
    bool is_confirm_time = false;

    int auto_reload_time = auto_enter_time;

    static OperatingTaskState* Instance()
    {
        static OperatingTaskState instance;
        return &instance;
    }
};
#endif