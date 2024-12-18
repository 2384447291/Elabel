#ifndef NOWIFISTATE_HPP
#define NOWIFISTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"

class NoWifiState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static NoWifiState* Instance()
    {
        static NoWifiState instance;
        return &instance;
    }
};
#endif