#ifndef ELABELCONTROLLER_HPP
#define ELABELCONTROLLER_HPP

#include "StateMachine.hpp"

class ElabelController;

class ElabelFsm : public StateMachine<ElabelController>
{
    private:
        bool hasInited;
    public:
        ElabelFsm(ElabelController * _pOwner) : StateMachine<ElabelController>(_pOwner){}
        void HandleInput();
        virtual void Init();
};

class ElabelController
{
    private:
        ElabelFsm m_elabelFsm;
    public:
        ElabelController();
        void Update();
        void Init();

        static ElabelController* Instance()
        {
            static ElabelController instance;
            return &instance;
        }
};


#endif