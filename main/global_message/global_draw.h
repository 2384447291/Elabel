#ifndef GLOBAL_DRAW_H
#define GLOBAL_DRAW_H
#include "../../components/ui/ui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    Btn_ChooseTask_big_height,
    Btn_ChooseTask_big_width,
    Btn_ChooseTask_small_height,
    Btn_ChooseTask_small_width,
} Button_type;

uint8_t get_button_size(Button_type button_type);

void lock_lvgl();
void release_lvgl();
void Inituilock();
void update_lvgl_task_list();
void guiTask(void *pvParameter);
void Change_All_language();
void switch_screen(lv_obj_t* new_screen);

void set_text_with_change_font(lv_obj_t * target_label,  const char * text, bool Is_bigger);
void set_text_without_change_font(lv_obj_t * target_label,  const char * text);

#ifdef __cplusplus
}
#endif

#endif