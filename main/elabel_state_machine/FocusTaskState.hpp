#ifndef FOCUSTASKSTATE_HPP
#define FOCUSTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

class FocusTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    int inner_time_countdown = 0;
    bool need_out_focus = false;

    uint8_t slave_unique_id = 0;

    static FocusTaskState* Instance()
    {
        static FocusTaskState instance;
        return &instance;
    }
};

#endif