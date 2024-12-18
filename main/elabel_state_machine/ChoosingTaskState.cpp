#include "ChoosingTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "ssd1680.h"
void choose_next_task()
{
    uint8_t temp_chosen_task = ElabelController::Instance()->ChosenTaskNum + 1;
    if(temp_chosen_task >= ElabelController::Instance()->TaskLength) temp_chosen_task = 0;
    ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
    ElabelController::Instance()->need_flash_paper = true;
}

void choose_previous_task()
{
    uint8_t temp_chosen_task = ElabelController::Instance()->ChosenTaskNum - 1;
    if(temp_chosen_task == 255) temp_chosen_task = ElabelController::Instance()->TaskLength - 1;
    ElabelController::Instance()->ChosenTaskNum = temp_chosen_task;
    ElabelController::Instance()->need_flash_paper = true;
}

void confirm_task()
{
    ChoosingTaskState::Instance()->is_confirm_task = true;
    ElabelController::Instance()->ChosenTaskId = get_global_data()->m_todo_list->items[ElabelController::Instance()->ChosenTaskNum].id;
}

void ChoosingTaskState::brush_task_list()
{
    update_lvgl_task_list();
    if(!need_stay_choosen) ElabelController::Instance()->ChosenTaskNum = 0;
    ElabelController::Instance()->TaskLength = get_global_data()->m_todo_list->size;
    set_task_list_state(newest);
}

void ChoosingTaskState::Init(ElabelController* pOwner)
{
    
}

void ChoosingTaskState::Enter(ElabelController* pOwner)
{
    //习惯性操作
    pOwner->need_flash_paper = false;

    is_confirm_task = false;
    lock_lvgl();
    //加载界面
    lv_scr_load(uic_TaskScreen);
    //更新任务列表ui
    brush_task_list();
    release_lvgl();

    //等待100ms组件加载完成
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ElabelController::Instance()->need_flash_paper = true;
    register_button_down_short_press_call_back(choose_next_task);
    register_button_up_short_press_call_back(choose_previous_task);
    register_button_down_long_press_call_back(confirm_task);
    register_button_up_long_press_call_back(confirm_task);
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.");
}

void ChoosingTaskState::Execute(ElabelController* pOwner)
{
    if(pOwner->need_flash_paper && pOwner->TaskLength > 0)
    {
        lock_lvgl();
        lv_obj_t *child = lv_obj_get_child(ui_Container3, pOwner->ChosenTaskNum + 1);
        if(child != NULL) {
            scroll_to_center(ui_Container3, child);
        }
        release_lvgl();
        pOwner->need_flash_paper = false;
    }
}

void ChoosingTaskState::Exit(ElabelController* pOwner)
{
    unregister_button_down_short_press_call_back(choose_next_task);
    unregister_button_up_short_press_call_back(choose_previous_task);
    unregister_button_down_long_press_call_back(confirm_task);
    unregister_button_up_long_press_call_back(confirm_task);
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}

// 滚动指定子对象到容器中心
void ChoosingTaskState::scroll_to_center(lv_obj_t *container, lv_obj_t *child) {
    // 获取容器和子对象的大小和位置
    lv_coord_t container_width = lv_obj_get_width(container);
    lv_coord_t container_height = lv_obj_get_height(container);

    lv_coord_t child_x = lv_obj_get_x(child);
    lv_coord_t child_y = lv_obj_get_y(child);
    lv_coord_t child_width = lv_obj_get_width(child);
    lv_coord_t child_height = lv_obj_get_height(child);

    // 计算滚动的偏移量（让子对象的中心与容器的中心对齐）
    lv_coord_t scroll_x = child_x + (child_width / 2) - (container_width / 2);
    lv_coord_t scroll_y = child_y + (child_height / 2) - (container_height / 2);

    // 滚动到计算的偏移量
    lv_obj_scroll_to(container, scroll_x, scroll_y, LV_ANIM_OFF);

    resize_task();
}

void ChoosingTaskState::resize_task()
{
    lv_area_t container_area;
    lv_obj_get_coords(ui_Container3, &container_area);
    lv_coord_t y_center = container_area.y1 + lv_area_get_height(&container_area) / 2;
    uint8_t child_count = lv_obj_get_child_cnt(ui_Container3);
    for(int i = 1; i<child_count - 1;i++)
    {
        lv_obj_t *child = lv_obj_get_child(ui_Container3, i);
        lv_area_t child_area;
        lv_obj_get_coords(child, &child_area);
        lv_coord_t child_y_center = child_area.y1 + lv_area_get_height(&child_area) / 2;
        lv_coord_t diff_y = LV_ABS(y_center - child_y_center);
        if(diff_y < 14)
        {
            lv_obj_set_size(child, 200, 36);
            lv_obj_set_style_bg_color(child, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            
            for(int j = 0;j<lv_obj_get_child_cnt(child);j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(child, j);
                lv_obj_set_style_text_font(label_child, &lv_font_montserrat_22, 0);
                lv_obj_set_style_text_color(label_child, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        else {
            lv_obj_set_size(child, 130, 24);
            lv_obj_set_style_bg_color(child, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

            for(int j = 0;j<lv_obj_get_child_cnt(child);j++)
            {
                lv_obj_t *label_child = lv_obj_get_child(child, j);
                lv_obj_set_style_text_font(label_child, &lv_font_montserrat_18, 0);
                lv_obj_set_style_text_color(label_child, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
}
