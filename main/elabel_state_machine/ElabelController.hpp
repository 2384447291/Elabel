#ifndef ELABELCONTROLLER_HPP
#define ELABELCONTROLLER_HPP

#include "StateMachine.hpp"
#include "global_message.h"

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
        
    public:
        ElabelFsm m_elabelFsm;
        ElabelController();
        void Update();
        void Init();
        APP_task_state APP_TASK(void);
        bool APP_FocusState_update(); 
        bool stop_mainTask = false;
        bool first_time_to_sync_tasklist = true;
        uint32_t TimeCountdownOffset;
        uint32_t TimeCountdown;
        uint8_t chosenTaskNum;
        bool needFlashEpaper;

        static ElabelController* Instance()
        {
            static ElabelController instance;
            return &instance;
        }
};


#endif