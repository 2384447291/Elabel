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

    static ActiveState* Instance()
    {
        static ActiveState instance;
        return &instance;
    }
};
#endif