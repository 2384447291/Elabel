#ifndef CHOOSINGTASKSTATE_HPP
#define CHOOSINGTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

class ChoosingTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static ChoosingTaskState* Instance()
    {
        static ChoosingTaskState instance;
        return &instance;
    }
};

#endif