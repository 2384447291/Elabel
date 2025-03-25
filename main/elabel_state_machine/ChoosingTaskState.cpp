#include "ChoosingTaskState.hpp"
#include "Esp_now_client.hpp"
#include "control_driver.hpp"
#include <cmath>

void choose_next_task()
{
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need choose next task");
        return;
    }
    else if(get_global_data()->m_todo_list->size==1)
    {
        ElabelController::Instance()->ChosenTaskNum = 0;
        ElabelController::Instance()->CenterTaskNum = 0;
    }
    else if(get_global_data()->m_todo_list->size==2)
    {
        if(ElabelController::Instance()->ChosenTaskNum == 0)
        {
            ElabelController::Instance()->ChosenTaskNum = 1;
            ElabelController::Instance()->CenterTaskNum = 1;
        }
        else if(ElabelController::Instance()->ChosenTaskNum == 1)
        {
            ElabelController::Instance()->ChosenTaskNum = 0;
            ElabelController::Instance()->CenterTaskNum = 0;
        }
    }
    else
    {
        int temp_chosen_task = ElabelController::Instance()->ChosenTaskNum + 1;
        if(temp_chosen_task < ElabelController::Instance()->TaskLength) 
        {
            ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
            if(ElabelController::Instance()->ChosenTaskNum > ElabelController::Instance()->CenterTaskNum + 1)
            {
                ElabelController::Instance()->CenterTaskNum = ElabelController::Instance()->CenterTaskNum + 1;
            }
        }
        else if(temp_chosen_task == ElabelController::Instance()->TaskLength)
        {
            ElabelController::Instance()->ChosenTaskNum = 0;
            ElabelController::Instance()->CenterTaskNum = 1;
        }
        ESP_LOGI("ChoosingTaskState","choose next task, ChosenTaskNum: %d, CenterTaskNum: %d", ElabelController::Instance()->ChosenTaskNum, ElabelController::Instance()->CenterTaskNum);
    }
    ChoosingTaskState::Instance()->need_flash_paper = true;
}

void choose_previous_task()
{
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need choose next task");
        return;
    }
    else if(get_global_data()->m_todo_list->size==1)
    {
        ElabelController::Instance()->ChosenTaskNum = 0;
        ElabelController::Instance()->CenterTaskNum = 0;
    }
    else if(get_global_data()->m_todo_list->size==2)
    {
        if(ElabelController::Instance()->ChosenTaskNum == 0)
        {
            ElabelController::Instance()->ChosenTaskNum = 1;
            ElabelController::Instance()->CenterTaskNum = 1;
        }
        else if(ElabelController::Instance()->ChosenTaskNum == 1)
        {
            ElabelController::Instance()->ChosenTaskNum = 0;
            ElabelController::Instance()->CenterTaskNum = 0;
        }
    }
    else
    {
        int temp_chosen_task = ElabelController::Instance()->ChosenTaskNum - 1;
        if(temp_chosen_task >= 0) 
        {
            ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
            if(ElabelController::Instance()->ChosenTaskNum < ElabelController::Instance()->CenterTaskNum - 1)
            {
                ElabelController::Instance()->CenterTaskNum = ElabelController::Instance()->CenterTaskNum - 1;
            }
        }
        else if(temp_chosen_task == -1)
        {
            ElabelController::Instance()->ChosenTaskNum = ElabelController::Instance()->TaskLength - 1;
            ElabelController::Instance()->CenterTaskNum = ElabelController::Instance()->TaskLength - 2;
        }
        ESP_LOGI("ChoosingTaskState","choose next task, ChosenTaskNum: %d, CenterTaskNum: %d", ElabelController::Instance()->ChosenTaskNum, ElabelController::Instance()->CenterTaskNum);
    }
    ChoosingTaskState::Instance()->need_flash_paper = true;
}

void jump_to_task_mode()
{
    if(ChoosingTaskState::Instance()->is_jump_to_task_mode || ChoosingTaskState::Instance()->is_jump_to_record_mode || ChoosingTaskState::Instance()->is_jump_to_time_mode) return;
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need confirm task");
        return;
    }
    ElabelController::Instance()->ChosenTaskId = get_global_data()->m_todo_list->items[ElabelController::Instance()->ChosenTaskNum].id;
    ChoosingTaskState::Instance()->is_jump_to_task_mode = true;
    ESP_LOGI("ChoosingTaskState","confirm task: %d",ElabelController::Instance()->ChosenTaskId);
}

void jump_to_record_mode()
{
    if(ChoosingTaskState::Instance()->is_jump_to_task_mode || ChoosingTaskState::Instance()->is_jump_to_record_mode || ChoosingTaskState::Instance()->is_jump_to_time_mode) return;
    ChoosingTaskState::Instance()->is_jump_to_record_mode = true;
    ESP_LOGI("ChoosingTaskState","jump to record mode");
}

void jump_to_time_mode()
{
    if(ChoosingTaskState::Instance()->is_jump_to_task_mode || ChoosingTaskState::Instance()->is_jump_to_record_mode || ChoosingTaskState::Instance()->is_jump_to_time_mode) return;
    ChoosingTaskState::Instance()->is_jump_to_time_mode = true;
    ESP_LOGI("ChoosingTaskState","jump to time mode");
}
void ChoosingTaskState::brush_task_list()
{
    update_lvgl_task_list(ElabelController::Instance()->CenterTaskNum);
    ElabelController::Instance()->TaskLength = get_global_data()->m_todo_list->size;
    ESP_LOGI("ChoosingTaskState","brush_task_list");
}

void ChoosingTaskState::Init(ElabelController* pOwner)
{
    
}

void ChoosingTaskState::Enter(ElabelController* pOwner)
{
    //只有在每次进入choosetask的时候或者退出focus的时候需要重置时间
    ElabelController::Instance()->TimeCountdown = TimeCountdownOffset;
    lock_lvgl();
    //加载界面
    switch_screen(ui_TaskScreen);
    brush_task_list();
    recolor_task();
    update_progress_bar();
    //更新任务列表ui
    release_lvgl();
    is_jump_to_task_mode = false;
    is_jump_to_record_mode = false;
    is_jump_to_time_mode = false;

    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.");
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(choose_previous_task);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(choose_next_task);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(jump_to_task_mode);

    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(jump_to_record_mode);

    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(jump_to_time_mode);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(jump_to_time_mode);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(jump_to_time_mode);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(jump_to_time_mode);
}

void ChoosingTaskState::Execute(ElabelController* pOwner)
{
    //这里的刷新只会刷新位置
    if(need_flash_paper)
    {
        lock_lvgl();
        brush_task_list();
        recolor_task();
        update_progress_bar();
        release_lvgl();
        need_flash_paper = false;
        ESP_LOGI("Scroll", "Scroll to center%d", pOwner->ChosenTaskNum);
    }
}

void ChoosingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(choose_previous_task);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(choose_next_task);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(jump_to_task_mode);

    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(jump_to_record_mode);

    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(jump_to_time_mode);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(jump_to_time_mode);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(jump_to_time_mode);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(jump_to_time_mode);
}

void ChoosingTaskState::recolor_task()
{
    uint8_t child_count = lv_obj_get_child_cnt(ui_TaskContainer);
        uint8_t chosen_task_button_index = ElabelController::Instance()->ChosenTaskNum - ElabelController::Instance()->CenterTaskNum + 1;
    for(int i = 0; i<child_count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(ui_TaskContainer, i);
        if(i == chosen_task_button_index)
        {
            lv_obj_set_style_bg_color(child, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t *ui_Label1 = lv_obj_get_child(child, 0);
            lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            for(int j = 0; j < lv_obj_get_child_cnt(ui_Label1); j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(ui_Label1, j);
                lv_obj_set_style_text_color(label_child, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        else {
            lv_obj_set_style_bg_color(child, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_t *ui_Label1 = lv_obj_get_child(child, 0);
            lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            for(int j = 0; j < lv_obj_get_child_cnt(ui_Label1); j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(ui_Label1, j);
                lv_obj_set_style_text_color(label_child, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
}

void ChoosingTaskState::update_progress_bar()
{
    //现在的任务数目
    uint8_t child_count = ElabelController::Instance()->TaskLength;
    //当前任务id
    uint8_t current_task_id = ElabelController::Instance()->ChosenTaskNum;
    //进度条的长度
    uint8_t progress_bar_length =  round(100.0f / (float)child_count);
    lv_arc_set_value(ui_Arc1, progress_bar_length);
    //进度条的起点
    uint8_t progress_bar_start = round((float)current_task_id / (float)child_count * 36.0f);
    lv_arc_set_bg_angles(ui_Arc1,0+progress_bar_start,36+progress_bar_start);
    // ESP_LOGI("update_progress_bar", "child_count: %d", child_count);
    // ESP_LOGI("update_progress_bar", "current_task_id: %d", current_task_id);
    // ESP_LOGI("update_progress_bar", "progress_bar_length: %d", progress_bar_length);
    // ESP_LOGI("update_progress_bar", "progress_bar_start: %d", progress_bar_start);
}
