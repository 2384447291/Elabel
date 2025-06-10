#ifndef GLOBAL_DRAW_H
#define GLOBAL_DRAW_H
#include "../../components/ui/ui.h"
#ifdef __cplusplus
extern "C" {
#endif
void lock_lvgl();
void release_lvgl();
void Inituilock();
void update_lvgl_task_list(int chose_task, uint8_t guide_page);
void guiTask(void *pvParameter);
void switch_screen(lv_obj_t* new_screen);
void set_text_without_change_font(lv_obj_t * target_label,  const char * text);

void Gui_init();
void suspend_gui();
void resume_gui();
#ifdef __cplusplus
}
#endif

#endif