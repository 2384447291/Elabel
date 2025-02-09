#ifndef GLOBAL_DRAW_H
#define GLOBAL_DRAW_H
#include "ui.h"
#ifdef __cplusplus
extern "C" {
#endif
void lock_lvgl();
void release_lvgl();
void Inituilock();
void update_lvgl_task_list();
void guiTask(void *pvParameter);
void set_text(lv_obj_t * target_label,  const char * text);
void set_text_without_change_font(lv_obj_t * target_label,  const char * text);
void set_text_chinese(lv_obj_t * target_label,  const char * text);
void set_text_english(lv_obj_t * target_label,  const char * text);
void Change_All_language();
#ifdef __cplusplus
}
#endif

#endif