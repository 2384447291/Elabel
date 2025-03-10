#include "ChoosingTaskState.hpp"
#include "Esp_now_client.hpp"
#include "control_driver.hpp"

void choose_next_task()
{
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need choose next task");
        return;
    }
    uint8_t temp_chosen_task = ElabelController::Instance()->ChosenTaskNum + 1;
    if(temp_chosen_task >= ElabelController::Instance()->TaskLength) temp_chosen_task = 0;
    ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
    ElabelController::Instance()->need_flash_paper = true;
}

void choose_previous_task()
{
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need choose previous task");
        return;
    }
    uint8_t temp_chosen_task = ElabelController::Instance()->ChosenTaskNum - 1;
    if(temp_chosen_task == 255) temp_chosen_task = ElabelController::Instance()->TaskLength - 1;
    ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
    ElabelController::Instance()->need_flash_paper = true;
}

void confirm_record()
{
    ElabelController::Instance()->is_confirm_record = true;
    ESP_LOGI("ChoosingTaskState","confirm record");
}


void confirm_task()
{
    if(get_global_data()->m_todo_list->size==0)
    {
        ESP_LOGE("ChoosingTaskState","No task no need confirm task");
        return;
    }
    ElabelController::Instance()->ChosenTaskId = get_global_data()->m_todo_list->items[ElabelController::Instance()->ChosenTaskNum].id;
    ESP_LOGI("ChoosingTaskState","confirm task: %d",ElabelController::Instance()->ChosenTaskId);
    ElabelController::Instance()->is_confirm_task = true;
}

void jump_to_activate()
{
    ChoosingTaskState::Instance()->is_jump_to_activate = true;
}

void ChoosingTaskState::brush_task_list()
{
    update_lvgl_task_list();
    if(!need_stay_choosen) ElabelController::Instance()->ChosenTaskNum = 0;
    ElabelController::Instance()->TaskLength = get_global_data()->m_todo_list->size;
}

void ChoosingTaskState::Init(ElabelController* pOwner)
{
    
}

void ChoosingTaskState::Enter(ElabelController* pOwner)
{
    pOwner->need_flash_paper = false;
    pOwner->is_confirm_task = false;
    pOwner->is_confirm_record = false;
    is_jump_to_activate = false;

    lock_lvgl();
    //加载界面
    switch_screen(ui_TaskScreen);
    //更新任务列表ui
    brush_task_list();
    release_lvgl();

    //等待100ms组件加载完成
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ElabelController::Instance()->need_flash_paper = true;
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.");
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(choose_previous_task);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(choose_next_task);

    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(confirm_record);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(confirm_record);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(confirm_record);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(confirm_record);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(confirm_task);

    ControlDriver::Instance()->button2.CallbackDoubleClick.registerCallback(jump_to_activate);
    ControlDriver::Instance()->button3.CallbackDoubleClick.registerCallback(jump_to_activate);
}

void ChoosingTaskState::Execute(ElabelController* pOwner)
{
    if(pOwner->need_flash_paper && pOwner->TaskLength > 0)
    {
        lock_lvgl();
        lv_obj_t *child = lv_obj_get_child(ui_HaveTaskContainer, pOwner->ChosenTaskNum + 1);
        if(child != NULL) {
            scroll_to_center(ui_HaveTaskContainer, child);
        }
        release_lvgl();
        pOwner->need_flash_paper = false;
        ESP_LOGI("Scroll", "Scroll to center%d", pOwner->ChosenTaskNum);
    }
}

void ChoosingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(choose_previous_task);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(choose_next_task);

    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(confirm_record);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(confirm_record);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(confirm_record);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(confirm_record);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(confirm_task);

    ControlDriver::Instance()->button2.CallbackDoubleClick.unregisterCallback(jump_to_activate);
    ControlDriver::Instance()->button3.CallbackDoubleClick.unregisterCallback(jump_to_activate);
}

// 滚动指定子对象到容器中心
void ChoosingTaskState::scroll_to_center(lv_obj_t *container, lv_obj_t *child) {
    lv_area_t child_area;
    lv_obj_get_coords(child, &child_area);
    lv_area_t container_area;
    lv_obj_get_coords(container, &container_area);
    lv_coord_t y_center = container_area.y1 + lv_area_get_height(&container_area) / 2;
    lv_coord_t child_y_center = child_area.y1 + lv_area_get_height(&child_area) / 2;
    lv_coord_t diff_y = y_center - child_y_center;

    // 滚动到计算的位置
    lv_obj_scroll_by(container, 0, diff_y, LV_ANIM_OFF);

    // 更新task大小
    resize_task();
}
void ChoosingTaskState::resize_task()
{
    lv_area_t container_area;
    lv_obj_get_coords(ui_HaveTaskContainer, &container_area);
    lv_coord_t y_center = container_area.y1 + lv_area_get_height(&container_area) / 2;
    uint8_t child_count = lv_obj_get_child_cnt(ui_HaveTaskContainer);
    for(int i = 1; i<child_count - 1;i++)
    {
        lv_obj_t *child = lv_obj_get_child(ui_HaveTaskContainer, i);
        lv_area_t child_area;
        lv_obj_get_coords(child, &child_area);
        lv_coord_t child_y_center = child_area.y1 + lv_area_get_height(&child_area) / 2;
        lv_coord_t diff_y = LV_ABS(y_center - child_y_center);
        //其实可以等于0，但是为了防止某些四舍五入的问题，所以设置为2
        if(diff_y < 2)
        {
            lv_obj_set_size(child, get_button_size(Btn_ChooseTask_big_width), get_button_size(Btn_ChooseTask_big_height));
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
            lv_obj_set_size(child, get_button_size(Btn_ChooseTask_small_width), get_button_size(Btn_ChooseTask_small_height));
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
