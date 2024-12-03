#ifndef STATEMACHINE_HPP
#define STATEMACHINE_HPP
#define STATEMACHINE "STATEMACHINE"

#include "State.hpp"
#include "esp_log.h"

template<typename controller_type>
class StateMachine
{
protected:
    controller_type* m_pOwner;
    State<controller_type>* m_pCurrentState;
    State<controller_type>* m_pPreviousState;

public:
    StateMachine(controller_type* m_pOwner) : m_pOwner(m_pOwner),
                                              m_pCurrentState(nullptr),
                                              m_pPreviousState(nullptr)
    {}

    void SetCurrentState(State<controller_type>* _state){ 
        m_pCurrentState = _state;
        }
    void SetPreviousState(State<controller_type>* _state){ m_pPreviousState = _state; }

    void Update()
    {
        if(m_pCurrentState)
        {
            m_pCurrentState->Execute(m_pOwner);
        }
    }

    void ChangeState(State<controller_type>* _state)
    {
        if(!_state)
        {
            return;
        }

        if(_state == m_pCurrentState)
        {
            return;
        }

        m_pPreviousState = m_pCurrentState;

        if(m_pPreviousState)
        {
            m_pPreviousState->Exit(m_pOwner);
        }

        m_pCurrentState = _state;

        if(m_pCurrentState)
        {
            m_pCurrentState->Enter(m_pOwner);
        }
    }

    State<controller_type>* GetCurrentState() const{ return m_pCurrentState; }
    State<controller_type>* GetPreviousState() const{ return m_pPreviousState; }

    virtual void Init() = 0;
};

#endif
