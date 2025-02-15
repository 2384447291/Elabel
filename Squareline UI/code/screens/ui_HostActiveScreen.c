// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "../ui.h"

void ui_HostActiveScreen_screen_init(void)
{
    ui_HostActiveScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_HostActiveScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_HostName = lv_label_create(ui_HostActiveScreen);
    lv_obj_set_width(ui_HostName, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_HostName, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_HostName, 0);
    lv_obj_set_y(ui_HostName, -45);
    lv_obj_set_align(ui_HostName, LV_ALIGN_CENTER);
    lv_label_set_text(ui_HostName, "Host");
    lv_obj_set_style_text_font(ui_HostName, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_HostInf = lv_label_create(ui_HostActiveScreen);
    lv_obj_set_width(ui_HostInf, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_HostInf, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_HostInf, 0);
    lv_obj_set_y(ui_HostInf, 10);
    lv_obj_set_align(ui_HostInf, LV_ALIGN_CENTER);
    lv_label_set_text(ui_HostInf, "Username:\nWIFIname:\nWIFIPassword:\nSlaveNum:");
    lv_obj_set_style_text_align(ui_HostInf, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_HostInf, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    uic_HostActiveScreen = ui_HostActiveScreen;
    uic_HostName = ui_HostName;
    uic_HostInf = ui_HostInf;

}
