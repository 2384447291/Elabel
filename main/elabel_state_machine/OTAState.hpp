#ifndef OTASTATE_HPP
#define OTASTATE_HPP
#include "StateMachine.hpp"
#include "ElabelController.hpp"

class OTAState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    bool is_ota_fail = false;
    uint8_t process = 0;

    static OTAState* Instance()
    {
        static OTAState instance;
        return &instance;
    }
};
#endif