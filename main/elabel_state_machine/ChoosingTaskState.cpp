#include "ChoosingTaskState.hpp"
#include "control_driver.h"
#include "global_message.h"
#include "OperatingTaskState.hpp"
#include "ChoosingTaskState.hpp"
#include "FocusTaskState.hpp"
#include "ssd1306.h"

void ChoosingTaskState::Init(ElabelController* pOwner)
{
    
}

void ChoosingTaskState::Enter(ElabelController* pOwner)
{
    //ui
    ElabelStateSet(ELSE_STATE);
    ESP_LOGI("fucking enter"," fuck you %d\n",pOwner->chosenTaskNum);
    _ui_screen_change(&uic_TaskScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_TaskScreen_screen_init);

    if(!tasklen)
    {
        _ui_flag_modify(ui_Container3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        _ui_flag_modify(ui_Container7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    else
    {
        _ui_flag_modify(ui_Container7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        _ui_flag_modify(ui_Container3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }

    pOwner->needFlashEpaper = true;
    ESP_LOGI(STATEMACHINE,"Enter ChoosingTaskState.\n");
}

void ChoosingTaskState::Execute(ElabelController* pOwner)
{
    if(lv_scr_act() != ui_TaskScreen)
    {
        ChoosingTaskState* pState = this;
        pState->Enter(pOwner);
        ESP_LOGI("choosing Task"," fuckyou %d\n",pOwner->chosenTaskNum);
        
        return;
    }

    EventBits_t bits;
    // 等待事件位pdTRUE 表示当 BUTTON_TASK_BIT 被设置时，函数将清除该位。
    //pdFALSE 表示不关心其他位的状态,只有BUTTON_TASK_BIT 被设置时才会被触发
    bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
    if (bits & BUTTON_TASK_BIT) {
        button_interrupt temp_interrupt = get_button_interrupt();
        if(temp_interrupt == short_press)
        {
            pOwner->m_elabelFsm.ChangeState(OperatingTaskState::Instance());
            return;
        }
    }

    if(tasklen)
    {
        if(bits & ENCODER_TASK_BIT)
        {
            encoder_interrupt temp_interrupt = get_encoder_interrupt();
            if(temp_interrupt == right_circle)
            {
                pOwner->chosenTaskNum++;
                if(pOwner->chosenTaskNum >= tasklen) pOwner->chosenTaskNum = 0;
            }
            else if(temp_interrupt == left_circle)
            {
                if(pOwner->chosenTaskNum == 0) pOwner->chosenTaskNum = tasklen - 1;
                else pOwner->chosenTaskNum--;
            }
            pOwner->needFlashEpaper = true;
        }
        if(elabelUpdateTick % 1000 == 0)
        {
            pOwner->chosenTaskNum++;
            if(pOwner->chosenTaskNum >= tasklen) pOwner->chosenTaskNum = 0;
            pOwner->needFlashEpaper = true;
            ESP_LOGI("ChoosingTaskState","chosenTaskNum: %d\n",pOwner->chosenTaskNum);
        }
    }

    if(tasklen && !last_tasklen)
    {
        _ui_flag_modify(ui_Container3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

        _ui_flag_modify(ui_Container7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    else if(!tasklen && last_tasklen)
    {
        _ui_flag_modify(ui_Container3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        _ui_flag_modify(ui_Container7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    last_tasklen = tasklen;

    if(pOwner->needFlashEpaper && tasklen > 0)
    {
        //调用lvgl刷新屏幕
        // lv_roller_set_selected(ui_Roller1, chosenTaskNum, LV_ANIM_OFF);
        lv_obj_t *child = lv_obj_get_child(ui_Container3, pOwner->chosenTaskNum + 1);
        if(child != NULL) {
            scroll_to_center(ui_Container3, child);
        }
        pOwner->needFlashEpaper = false;
    }
    // _ui_screen_change(&uic_TaskScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_TaskScreen_screen_init);
}

void ChoosingTaskState::Exit(ElabelController* pOwner)
{
    ESP_LOGI(STATEMACHINE,"Out ChoosingTaskState.\n");
}

// 滚动指定子对象到容器中心
void ChoosingTaskState::scroll_to_center(lv_obj_t *container, lv_obj_t *child) {
    if (container == NULL || child == NULL) {
        printf("Container or child is NULL\n");
        return;
    }

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

    printf("Scrolled to center child at x: %d, y: %d\n", scroll_x, scroll_y);
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
