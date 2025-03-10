// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "../ui.h"

void ui_OperatingScreen_screen_init(void)
{
    ui_OperatingScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_OperatingScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Panel10 = lv_obj_create(ui_OperatingScreen);
    lv_obj_set_width(ui_Panel10, 250);
    lv_obj_set_height(ui_Panel10, 122);
    lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel10, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel10, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel10, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_OperatingTime = lv_label_create(ui_Panel10);
    lv_obj_set_width(ui_OperatingTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_OperatingTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_OperatingTime, 1);
    lv_obj_set_y(ui_OperatingTime, 0);
    lv_obj_set_align(ui_OperatingTime, LV_ALIGN_CENTER);
    lv_label_set_text(ui_OperatingTime, "5:00");
    lv_obj_set_style_text_color(ui_OperatingTime, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_OperatingTime, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_OperatingTime, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Time2 = lv_label_create(ui_OperatingTime);
    lv_obj_set_width(ui_Time2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Time2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Time2, -1);
    lv_obj_set_y(ui_Time2, 0);
    lv_obj_set_align(ui_Time2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Time2, "5:00");
    lv_obj_set_style_text_color(ui_Time2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Time2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Time2, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Time3 = lv_label_create(ui_OperatingTime);
    lv_obj_set_width(ui_Time3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Time3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Time3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Time3, "5:00");
    lv_obj_set_style_text_font(ui_Time3, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_OperatingLoadingpanel = lv_obj_create(ui_OperatingScreen);
    lv_obj_set_width(ui_OperatingLoadingpanel, 250);
    lv_obj_set_height(ui_OperatingLoadingpanel, 122);
    lv_obj_add_flag(ui_OperatingLoadingpanel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_OperatingLoadingpanel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_OperatingLoadingpanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_OperatingLoadingpanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_OperatingLoadingpanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_OperatingLoadingpanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_OperatingLoadingpanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_OperatingTime2 = lv_label_create(ui_OperatingLoadingpanel);
    lv_obj_set_width(ui_OperatingTime2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_OperatingTime2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_OperatingTime2, 1);
    lv_obj_set_y(ui_OperatingTime2, 0);
    lv_obj_set_align(ui_OperatingTime2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_OperatingTime2, "同步中...");
    lv_obj_set_style_text_color(ui_OperatingTime2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_OperatingTime2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_OperatingTime2, &ui_font_Chinese_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Time5 = lv_label_create(ui_OperatingTime2);
    lv_obj_set_width(ui_Time5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Time5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Time5, -1);
    lv_obj_set_y(ui_Time5, 0);
    lv_obj_set_align(ui_Time5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Time5, "同步中...");
    lv_obj_set_style_text_color(ui_Time5, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Time5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Time5, &ui_font_Chinese_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Time1 = lv_label_create(ui_OperatingTime2);
    lv_obj_set_width(ui_Time1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Time1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Time1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Time1, "同步中...");
    lv_obj_set_style_text_font(ui_Time1, &ui_font_Chinese_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    uic_OperatingScreen = ui_OperatingScreen;
    uic_OperatingTime = ui_OperatingTime;
    uic_OperatingTime = ui_OperatingTime2;

}
