// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "../ui.h"

void ui_HalfmindScreen_screen_init(void)
{
    ui_HalfmindScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_HalfmindScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image1 = lv_img_create(ui_HalfmindScreen);
    lv_img_set_src(ui_Image1, &ui_img_halfmind_png);
    lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);   /// 165
    lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT);    /// 65
    lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    uic_HalfmindScreen = ui_HalfmindScreen;

}
