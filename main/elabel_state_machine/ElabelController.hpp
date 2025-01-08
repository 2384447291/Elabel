#ifndef ELABELCONTROLLER_HPP
#define ELABELCONTROLLER_HPP

#include "StateMachine.hpp"
#include "global_message.h"
#include "global_time.h"
#include "global_draw.h"
#include "control_driver.hpp"
#include "ui.h"
#define TimeCountdownOffset 300
#define second_perencoder 150
#define MAX_time 3600

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

        //所选择的task的id(一定要以id来传递，不要用number来传递，id是唯一的)
        uint16_t ChosenTaskId;
        
        //任务倒计时的实际时间
        uint32_t TimeCountdown;
        //所选择的task在当前列表中的位置
        uint16_t ChosenTaskNum;
        //当前task_length的长度
        uint16_t TaskLength;
        //刷新标记
        bool need_flash_paper = false;
        //是否需要主动退出focus
        bool focus_by_myself = false;

        static ElabelController* Instance()
        {
            static ElabelController instance;
            return &instance;
        }
};


#endif