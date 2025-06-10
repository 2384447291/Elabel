#ifndef  INFOSTATE_HPP
#define  INFOSTATE_HPP

#include "StateMachine.hpp"
#include <cmath>
#include "ElabelController.hpp"
#include "battery_manager.hpp"

enum InfoType
{
    INFO_TYPE_DEVICE = 1,
    INFO_TYPE_CONNECTION,
    INFO_TYPE_OTA,
    INFO_TYPE_REBOOT,
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

    bool need_back_choose_task = false;
    bool need_forward_ota = false;

    static InfoState* Instance()
    {
        static InfoState instance;
        return &instance;
    }

    void update_bar()
    {
        //现在的任务数目
        uint8_t child_count = INFO_TYPE_NUM - 2;
        //当前任务id
        uint8_t current_task_id = m_info_type - 1;
        if(m_info_type == INFO_TYPE_REBOOT)
        {
            current_task_id = INFO_TYPE_OTA -1;
        }
        //进度条的长度
        uint8_t progress_bar_length =  round(100.0f / (float)child_count);
        lv_arc_set_value(ui_Arc3, progress_bar_length);
        //进度条的起点
        uint8_t progress_bar_start = round((float)current_task_id / (float)child_count * 36.0f);
        lv_arc_set_bg_angles(ui_Arc3,0+progress_bar_start,36+progress_bar_start);
    }

    void show_info()
    {
        //隐藏按键
        lv_obj_add_flag(ui_OTA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Reboot, LV_OBJ_FLAG_HIDDEN);

        //显示被按键遮挡的页表
        lv_obj_clear_flag(ui_Message3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Message5, LV_OBJ_FLAG_HIDDEN);
    }

    void show_button()
    {
        //显示按键
        lv_obj_clear_flag(ui_OTA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Reboot, LV_OBJ_FLAG_HIDDEN);

        //隐藏被按键遮挡的页表
        lv_obj_add_flag(ui_Message3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Message4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Message5, LV_OBJ_FLAG_HIDDEN);
    }

    //---------------(可能会变化)需要展示的信息-----------------//
    //时间
    char Live_time[30];
    //连接强度
    int16_t connect_strength;

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

    void flush_connection_info()
    {
        show_info();
        set_text_without_change_font(ui_Messageguide, "Connection Info");
        if(get_global_data()->m_is_host == 1)
        {
            char stringUserName[40] = "User Name:";
            strcat(stringUserName, get_global_data()->m_userName);
            set_text_without_change_font(ui_Message1, stringUserName);

            char stringWifiName[40] = "WIFI Name:";
            strcat(stringWifiName, get_global_data()->m_wifi_ssid);
            set_text_without_change_font(ui_Message2, stringWifiName);
            
            char stringUpTime[40];
            
            get_open_time(Live_time);
            snprintf(stringUpTime, 40, "Up Time: %s", Live_time);
            set_text_without_change_font(ui_Message3, stringUpTime);

            set_text_without_change_font(ui_Message4, "Mode: Host");

            int satellite_counts = get_global_data()->m_slave_num;
            char stringSatelliteCounts[40];
            sprintf(stringSatelliteCounts, "%s %d", "Satellite Counts: ", satellite_counts);
            set_text_without_change_font(ui_Message5, stringSatelliteCounts);
        }
        else if(get_global_data()->m_is_host == 2)
        {
            char stringUserName[40] = "User Name:";
            strcat(stringUserName, get_global_data()->m_userName);
            set_text_without_change_font(ui_Message1, stringUserName);

            char stringWifiName[40] = "WIFI Name:";
            strcat(stringWifiName, get_global_data()->m_wifi_ssid);
            set_text_without_change_font(ui_Message2, stringWifiName);

            char stringUpTime[40];
            char Time[30];
            get_open_time(Time);
            snprintf(stringUpTime, 40, "Up Time: %s", Time);
            set_text_without_change_font(ui_Message3, stringUpTime);

            set_text_without_change_font(ui_Message4, "Mode: Satellite");

            char stringSignalStrength[40];
            sprintf(stringSignalStrength, "Signal Strength: %d", connect_strength);
            set_text_without_change_font(ui_Message5, stringSignalStrength);
        }
    }

    void flush_power_info_ota()
    {
        show_button();
        set_text_without_change_font(ui_Messageguide, "Power Info");

        float battery_level = BatteryManager::Instance()->getBatteryLevel();
        char stringBattery[40] = "Battery Power:";
        sprintf(stringBattery, "%s %d %s", "Battery Power: ", (int)battery_level, "V");
        set_text_without_change_font(ui_Message2, stringBattery);

        if(battery_level < 1.0f || battery_level > 4.6f)
        {
            char stringPower[40] = "Cable Power: Connected";
            set_text_without_change_font(ui_Message1, stringPower);
        } 
        else
        {
            char stringPower[40] = "Cable Power: Discennected";
            set_text_without_change_font(ui_Message1, stringPower);            
        }

        lv_obj_add_state(ui_OTA, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_Reboot, LV_STATE_PRESSED );
    }

    void flush_power_info_reboot()
    {
        show_button();
        set_text_without_change_font(ui_Messageguide, "Power Info");

        float battery_level = BatteryManager::Instance()->getBatteryLevel();
        char stringBattery[40] = "Battery Power:";
        sprintf(stringBattery, "%s %d %s", "Battery Power: ", (int)battery_level, "V");
        set_text_without_change_font(ui_Message2, stringBattery);

        if(battery_level < 1.0f || battery_level > 4.6f)
        {
            char stringPower[40] = "Cable Power: Connected";
            set_text_without_change_font(ui_Message1, stringPower);
        } 
        else
        {
            char stringPower[40] = "Cable Power: Disconnected";
            set_text_without_change_font(ui_Message1, stringPower);            
        }

        lv_obj_clear_state(ui_OTA, LV_STATE_PRESSED );
        lv_obj_add_state(ui_Reboot, LV_STATE_PRESSED );
    }
};

#endif