#include "OperatingTaskState.hpp"

void OperatingTaskState::Init(ElabelController* pOwner)
{
    
}

void OperatingTaskState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.\n");
}

void OperatingTaskState::Execute(ElabelController* pOwner)
{
    
}

void OperatingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}