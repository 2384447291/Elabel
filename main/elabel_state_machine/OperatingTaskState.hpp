#ifndef OPERATINGTASKSTATE_HPP
#define OPERATINGTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

class OperatingTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    int last_encoder_value = 0;
    bool is_confirm_time;
    bool is_not_confirm_task = 0;
    int update_lock = 0;
    int auto_reload_time;
    static OperatingTaskState* Instance()
    {
        static OperatingTaskState instance;
        return &instance;
    }
};
#endif