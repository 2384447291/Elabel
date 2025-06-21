#ifndef ELABELCONTROLLER_HPP
#define ELABELCONTROLLER_HPP

#include "StateMachine.hpp"
#include "global_message.h"
#include "global_time.h"
#include "global_draw.h"
#include "global_nvs.h"
#include "control_driver.hpp"
#define MAX_time 3600

void force_reset_elabel();

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
        uint16_t ChosenTaskId = 0;
        //任务倒计时的实际时间
        uint32_t TimeCountdown = (get_global_data()->m_device_info.default_counter_time*60);
        //所选择的task在当前列表中的位置
        uint16_t ChosenTaskNum = 0;
        //处在中心的task在列表中的位置
        uint16_t CenterTaskNum = 0;
        //当前task_length的长度
        uint16_t TaskLength = 0;

        //防止卡死的标签
        int32_t stuck_time = 0;

        static ElabelController* Instance()
        {
            static ElabelController instance;
            return &instance;
        }
};


#endif