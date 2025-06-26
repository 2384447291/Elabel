#ifndef INITSTATE_HPP
#define INITSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp" 
    
class InitState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool is_init = false;
    bool need_enter_ota = false;

    static InitState* Instance()
    {
        static InitState instance;
        return &instance;
    }
};

#endif