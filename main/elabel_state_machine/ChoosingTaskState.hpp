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
    void scroll_to_center(lv_obj_t *container, lv_obj_t *child);
    void resize_task();

    bool need_stay_choosen = false;
    bool is_confirm_task = false;
    bool is_jump_to_activate = false;


    static ChoosingTaskState* Instance()
    {
        static ChoosingTaskState instance;
        return &instance;
    }
};


#endif