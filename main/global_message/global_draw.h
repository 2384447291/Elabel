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
void shutdown_screen();
#ifdef __cplusplus
}
#endif

#endif