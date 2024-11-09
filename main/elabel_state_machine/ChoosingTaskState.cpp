#include "ChoosingTaskState.hpp"

void ChoosingTaskState::Init(ElabelController* pOwner)
{
    
}

void ChoosingTaskState::Enter(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.\n");
}

void ChoosingTaskState::Execute(ElabelController* pOwner)
{

}

void ChoosingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}