#ifndef InitState_HPP
#define InitState_HPP

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
    
    uint8_t is_need_ota = 0;//0:没有选择,1:选择ota,2:选择不ota
    bool button_choose_ota_left = true;
    uint32_t ota_Wait_tick = 0;

    static InitState* Instance()
    {
        static InitState instance;
        return &instance;
    }
};

#endif