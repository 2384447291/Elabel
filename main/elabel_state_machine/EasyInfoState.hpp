#ifndef  EASYINFOSTATE_HPP
#define  EASYINFOSTATE_HPP

#include "StateMachine.hpp"
#include <cmath>
#include "ElabelController.hpp"

class EasyInfoState : public State<ElabelController>
{
public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    static EasyInfoState* Instance()
    {
        static EasyInfoState instance;
        return &instance;
    }

    void show_info()
    {
        //隐藏按键
        lv_obj_add_flag(ui_OTA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Reboot, LV_OBJ_FLAG_HIDDEN);

        //显示被按键遮挡的页表
        lv_obj_clear_flag(ui_Messageguide, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message5, LV_OBJ_FLAG_HIDDEN);

        lv_arc_set_value(ui_Arc3, 100);
        lv_arc_set_bg_angles(ui_Arc3,0,36);
    }

    void flush_device_info()
    {
        show_info();
        set_text_without_change_font(ui_Messageguide, "Device Info");

        set_text_without_change_font(ui_Message1, "Name:Halfmind Reminder");

        char stringModel[40] = "Model:";
        strcat(stringModel, DEVICE_MODEL);
        set_text_without_change_font(ui_Message2, stringModel);

        char stringFirmware[40] = "Firmware:";
        strcat(stringFirmware, FIRMWARE_VERSION);
        set_text_without_change_font(ui_Message3, stringFirmware);

        char stringLanguage[40] = "Language:";
        strcat(stringLanguage, LANGUAGE);
        set_text_without_change_font(ui_Message4, stringLanguage);

        char stringSN[40] = "SN:";
        strcat(stringSN, get_global_data()->m_mac_str);
        set_text_without_change_font(ui_Message5, stringSN);
    }
};

#endif