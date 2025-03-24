#ifndef CHOOSINGTASKSTATE_HPP
#define CHOOSINGTASKSTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "global_time.h"

class ChoosingTaskState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    void brush_task_list();
    void recolor_task();
    void update_progress_bar();

    bool is_jump_to_task_mode = false;
    bool is_jump_to_time_mode = false;
    bool is_jump_to_record_mode = false;

    //刷新标记
    bool need_flash_paper = false;

    static ChoosingTaskState* Instance()
    {
        static ChoosingTaskState instance;
        return &instance;
    }
};


#endif