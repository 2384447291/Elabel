#include "SleepState.hpp"
#include "control_driver.hpp"
void SleepState::Init(ElabelController* pOwner)
{
}

void SleepState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter SleepState.");
    need_out_state = false;
    prepare_sleep();
    enter_sleep();
}
void SleepState::Execute(ElabelController* pOwner)
{
    
}

void SleepState::Exit(ElabelController* pOwner)
{
}

