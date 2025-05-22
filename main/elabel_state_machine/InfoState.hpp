#ifndef  INFOSTATE_HPP
#define  INFOSTATE_HPP

#include "StateMachine.hpp"
#include <cmath>
#include "ElabelController.hpp"

enum InfoType
{
    INFO_TYPE_DEVICE = 1,
    INFO_TYPE_CONNECTION,
    INFO_TYPE_NUM
};

class InfoState : public State<ElabelController>
{
public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    InfoType m_info_type = INFO_TYPE_DEVICE;
    bool need_flash_paper = false;
    bool need_out_state = false;

    static InfoState* Instance()
    {
        static InfoState instance;
        return &instance;
    }

    void update_bar()
    {
        //现在的任务数目
        uint8_t child_count = INFO_TYPE_NUM - 1;
        //当前任务id
        uint8_t current_task_id = m_info_type - 1;
        //进度条的长度
        uint8_t progress_bar_length =  round(100.0f / (float)child_count);
        lv_arc_set_value(ui_Arc3, progress_bar_length);
        //进度条的起点
        uint8_t progress_bar_start = round((float)current_task_id / (float)child_count * 36.0f);
        lv_arc_set_bg_angles(ui_Arc3,0+progress_bar_start,36+progress_bar_start);
    }

    void flush_device_info()
    {
        set_text_without_change_font(ui_Messageguide, "Device Info");

        set_text_without_change_font(ui_Message1, "Name:Halfmind Reminder");
        set_text_without_change_font(ui_Message2, "Model:R01A");

        char stringFirmware[40] = "Firmware:";
        strcat(stringFirmware, FIRMWARE_VERSION);
        set_text_without_change_font(ui_Message3, stringFirmware);

        set_text_without_change_font(ui_Message4, "Language:English");
        char stringSN[40] = "SN:";
        strcat(stringSN, get_global_data()->m_mac_str);
        set_text_without_change_font(ui_Message5, stringSN);
    }

    void flush_connection_info()
    {
        set_text_without_change_font(ui_Messageguide, "Connection Info");
        if(get_global_data()->m_is_host)
        {
            char stringUserName[40] = "User Name:";
            strcat(stringUserName, get_global_data()->m_userName);
            set_text_without_change_font(ui_Message1, stringUserName);

            char stringWifiName[40] = "WIFI Name:";
            strcat(stringWifiName, get_global_data()->m_wifi_ssid);
            set_text_without_change_font(ui_Message2, stringWifiName);

            set_text_without_change_font(ui_Message3, "Up Time: 3D 2H 12M");

            set_text_without_change_font(ui_Message4, "Mode: Host");

            int satellite_counts = get_global_data()->m_slave_num;
            char stringSatelliteCounts[40];
            sprintf(stringSatelliteCounts, "%s %d", "Satellite Counts: ", satellite_counts);
            set_text_without_change_font(ui_Message5, stringSatelliteCounts);
        }
        else
        {
            char stringUserName[40] = "User Name:";
            strcat(stringUserName, get_global_data()->m_userName);
            set_text_without_change_font(ui_Message1, stringUserName);

            set_text_without_change_font(ui_Message2, "WIFI Name: -----");

            set_text_without_change_font(ui_Message3, "Up Time: 3D 2H 12M");

            set_text_without_change_font(ui_Message4, "Mode: Satellite");

            set_text_without_change_font(ui_Message5, "Signal Strength: 24/100");
        }
    }

};

#endif