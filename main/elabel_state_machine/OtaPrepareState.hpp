#ifndef OTAPREPARESTATE_HPP
#define OTAPREPARESTATE_HPP

#include "StateMachine.hpp"
#include "ElabelController.hpp"
#include "network.h"
#include "global_draw.h"
#include "http.h"
#include "math.h"

#define RECONNECT_COUNT_DOWN 30
#define OTA_WAIT_TICK 300
#define DESCRIPTION_SPLIT "-"
#define DESCRIPTION_LENGTH 128
#define DESCRIPTION_NUM 8

typedef enum
{
    default_ota_prepare_process,
    ota_prepare_connecting_wifi_process,
    ota_prepare_disconnecting_wifi_process,
    ota_prepare_success_connect_wifi_process,
    ota_prepare_decide_ota_process,
    ota_prepare_no_need_ota_process,
    ota_prepare_need_to_ota_process,

    ota_prepare_ota_finish_process,
} ota_prepare_process;

class OtaPrepareState : public State<ElabelController>
{
private:

public:
    virtual void Init(ElabelController* pOwner);
    virtual void Enter(ElabelController* pOwner);
    virtual void Execute(ElabelController* pOwner);
    virtual void Exit(ElabelController* pOwner);

    ota_prepare_process m_ota_prepare_process = default_ota_prepare_process;

    //进入这个状态机只有两条路重启或者开始ota
    bool button_ota_prepare_choose_left = true;
    bool need_flash_paper = false;
    bool need_enter_ota = false;
    bool need_out_ota_prepare = false;
    char description[DESCRIPTION_NUM][DESCRIPTION_LENGTH];

    
    uint16_t ota_prepare_wait_tick = 0;
    uint8_t reconnect_count_down = RECONNECT_COUNT_DOWN;
    int current_step = 0;
    uint8_t all_step = 0;

    void update_ota_describtion()
    {
        ESP_LOGI("OtaPrepareState", "update_ota_describtion, current_step: %d, all_step: %d", current_step, all_step);
        if(current_step == all_step)
        {
            lv_obj_add_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
            lv_obj_clear_state(ui_OTAButtonStart, LV_STATE_PRESSED );
            set_text_without_change_font(ui_VersionDescribtion, description[all_step - 1]);
        }
        else if(current_step == all_step + 1)
        {
            lv_obj_clear_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
            lv_obj_add_state(ui_OTAButtonStart, LV_STATE_PRESSED );
            set_text_without_change_font(ui_VersionDescribtion, description[all_step - 1]);
        }
        else 
        {
            lv_obj_clear_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
            lv_obj_clear_state(ui_OTAButtonStart, LV_STATE_PRESSED );
            set_text_without_change_font(ui_VersionDescribtion, description[current_step]);
        }
        //现在的任务数目
        uint8_t child_count = all_step;
        //当前任务id
        uint8_t current_task_id = current_step;
        if(current_task_id >= all_step)
        {
            current_task_id = all_step - 1;
        }
        //进度条的长度
        uint8_t progress_bar_length = round(100.0f / (float)child_count);
        lv_arc_set_value(ui_Arc4, progress_bar_length);
        //进度条的起点
        uint8_t progress_bar_start = round((float)current_task_id / (float)child_count * 36.0f);
        lv_arc_set_bg_angles(ui_Arc4,0+progress_bar_start,36+progress_bar_start);
    }

    void check_firmware_content()
    {
        memset(description, 0, sizeof(description));
        all_step = 0;
        
        char content[sizeof(get_global_data()->m_content)];
        memcpy(content, get_global_data()->m_content, sizeof(get_global_data()->m_content));
        
        char* start = content;
        char* token = strstr(start, DESCRIPTION_SPLIT);
        
        // 跳过开头的分隔符
        if (*start == *DESCRIPTION_SPLIT) {
            start++;
            token = strstr(start, DESCRIPTION_SPLIT);
        }
        
        while (token != NULL && all_step < DESCRIPTION_NUM) {
            size_t len = token - start;
            if (len > 0 && len < DESCRIPTION_LENGTH - 1) { // -1 为了给分隔符留空间
                // 添加分隔符到开头
                description[all_step][0] = *DESCRIPTION_SPLIT;
                strncpy(description[all_step] + 1, start, len);
                description[all_step][len + 1] = '\0';
                all_step++;
            }
            
            start = token + 1;
            token = strstr(start, DESCRIPTION_SPLIT);
        }
        
        // 处理最后一个描述
        if (*start != '\0' && all_step < DESCRIPTION_NUM) {
            size_t len = strlen(start);
            if (len > 0 && len < DESCRIPTION_LENGTH - 1) {
                description[all_step][0] = *DESCRIPTION_SPLIT;
                strncpy(description[all_step] + 1, start, len);
                description[all_step][len + 1] = '\0';
                all_step++;
            }
        }

        ESP_LOGI("OtaPrepareState", "Describtion split in %d", all_step);
        for(int i = 0; i < all_step; i++)
        {
            ESP_LOGI("OtaPrepareState", "Describtion %d: %s", i, description[i]);
        }
    }

    //----------------------------连接wifi阶段-------------------------------//
    void enter_connect_wifi()
    {
        m_wifi_disconnect();
        m_wifi_connect();
        need_flash_paper = false;

        reconnect_count_down = RECONNECT_COUNT_DOWN;
        m_ota_prepare_process = ota_prepare_connecting_wifi_process;
        
        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        lv_obj_clear_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        set_text_without_change_font(ui_WIFIname, get_global_data()->m_wifi_ssid);
        char time_str[40];
        sprintf(time_str, "Timeout in %d secs", reconnect_count_down);
        set_text_without_change_font(ui_HostActiveAutoTime, time_str);
        release_lvgl();
    }

    void enter_disconnect_wifi()
    {
        m_ota_prepare_process = ota_prepare_disconnecting_wifi_process;
        //禁止重新连接的断连
        m_wifi_disconnect();
        need_flash_paper = false;

        lock_lvgl();

        switch_screen(ui_HostActiveScreen);
        set_text_without_change_font(ui_Disconnectwifiname, get_global_data()->m_wifi_ssid);

        button_ota_prepare_choose_left = true;
        lv_obj_add_state(ui_HostActiveCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_HostActiveRetry, LV_STATE_PRESSED );

        lv_obj_add_flag(ui_ConnectingWIFI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_DisconnectWIFI, LV_OBJ_FLAG_HIDDEN);
        
        release_lvgl();
    }

    void enter_success_connect_wifi()
    {
        m_ota_prepare_process = ota_prepare_success_connect_wifi_process;
        lock_lvgl();
        switch_screen(ui_HostActiveScreen);
        set_text_without_change_font(ui_HostActiveAutoTime, "Success!!!");
        release_lvgl();
        enter_decide_ota();
    }
    //----------------------------连接wifi阶段-------------------------------//


    //----------------------------尝试ota阶段-------------------------------//
    void enter_decide_ota()
    {
        m_ota_prepare_process = ota_prepare_decide_ota_process;

        lock_lvgl();

        switch_screen(ui_OTAScreen);

        //隐藏ota按钮
        lv_obj_add_flag(ui_scrollbar3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_VersionDescribtion, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_OTAButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_RetryCheckVersionButton, LV_OBJ_FLAG_HIDDEN);
        //显示提示字
        lv_obj_clear_flag(ui_OTAWaitingGuide, LV_OBJ_FLAG_HIDDEN);

        set_text_without_change_font(ui_NewFirmware, "Checking Update ...");

        set_text_without_change_font(ui_OTAWaitingGuide, "Please Waiting");

        char version_str[150];
        sprintf(version_str, "V %s--------->V ???", FIRMWARE_VERSION);
        set_text_without_change_font(ui_VersionChange, version_str);

        release_lvgl();

        bool get_firmware_need_update = http_get_latest_version(true);

        if(get_firmware_need_update && strlen(get_global_data()->m_newest_firmware_url) != 0)
        {
            //如果firmware需要更新
            if(strcmp(get_global_data()->m_version, FIRMWARE_VERSION) != 0)
            {
                enter_need_to_ota();
            }
            else
            {
                enter_no_need_ota(true);
            }
        }
        //如果没有获取到最新版本固件
        else
        {
            enter_no_need_ota(false);
        }
        //如果判断为需要OTA
    }

    //有两种形式，一种是获取链接失败，一种是确实不需要ota
    void enter_no_need_ota(bool is_get_firmware_need_update)
    {
        m_ota_prepare_process = ota_prepare_no_need_ota_process;
        need_flash_paper = false;
        if(is_get_firmware_need_update)
        {
            lock_lvgl();

            switch_screen(ui_OTAScreen);

            set_text_without_change_font(ui_NewFirmware, "Up to date");

            char version_str[150];
            sprintf(version_str, "V %s--------->V %s", FIRMWARE_VERSION, get_global_data()->m_version);
            set_text_without_change_font(ui_VersionChange, version_str);

            //隐藏ota按钮
            lv_obj_add_flag(ui_scrollbar3, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_VersionDescribtion, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_OTAButton, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_RetryCheckVersionButton, LV_OBJ_FLAG_HIDDEN);
            //隐藏提示字
            lv_obj_add_flag(ui_OTAWaitingGuide, LV_OBJ_FLAG_HIDDEN);

            button_ota_prepare_choose_left = true;
            lv_obj_add_state(ui_RetryCheckVersionButtonCancel, LV_STATE_PRESSED );
            lv_obj_clear_state(ui_RetryCheckVersionButtonRetry, LV_STATE_PRESSED );

            release_lvgl();           
        }
        else
        {
            lock_lvgl();

            switch_screen(ui_OTAScreen);

            set_text_without_change_font(ui_NewFirmware, "Fail Get Update Data");

            char version_str[150];
            sprintf(version_str, "V %s--------->V ???", FIRMWARE_VERSION);
            set_text_without_change_font(ui_VersionChange, version_str);

            //隐藏ota按钮
            lv_obj_add_flag(ui_scrollbar3, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_VersionDescribtion, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_OTAButton, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_RetryCheckVersionButton, LV_OBJ_FLAG_HIDDEN);
            //隐藏提示字
            lv_obj_add_flag(ui_OTAWaitingGuide, LV_OBJ_FLAG_HIDDEN);

            button_ota_prepare_choose_left = true;
            lv_obj_add_state(ui_RetryCheckVersionButtonCancel, LV_STATE_PRESSED );
            lv_obj_clear_state(ui_RetryCheckVersionButtonRetry, LV_STATE_PRESSED );

            release_lvgl();
        }
    }

    void enter_need_to_ota()
    {
        m_ota_prepare_process = ota_prepare_need_to_ota_process;
        ota_prepare_wait_tick = OTA_WAIT_TICK;
        need_flash_paper = false;
        check_firmware_content();
        current_step = 0;
        lock_lvgl();

        switch_screen(ui_OTAScreen);

        set_text_without_change_font(ui_NewFirmware, "Update Available");

        char version_change[150];
        sprintf(version_change, "V %s--------->V %s", FIRMWARE_VERSION, get_global_data()->m_version);
        set_text_without_change_font(ui_VersionChange, version_change);

        update_ota_describtion();

        //隐藏ota按钮
        lv_obj_clear_flag(ui_scrollbar3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_VersionDescribtion, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_OTAButton, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_RetryCheckVersionButton, LV_OBJ_FLAG_HIDDEN);
        //隐藏提示字
        lv_obj_add_flag(ui_OTAWaitingGuide, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_state(ui_OTAButtonCancel, LV_STATE_PRESSED );
        lv_obj_clear_state(ui_OTAButtonStart, LV_STATE_PRESSED );

        release_lvgl();
    }

    void enter_ota_finish(bool _need_enter_ota)
    {
        m_ota_prepare_process = ota_prepare_ota_finish_process;
        if(_need_enter_ota)
        {
            need_enter_ota = true;
            need_out_ota_prepare = false;
        }
        //如果不需要ota
        else
        {
            //如果是从机还要断开wifi
            if(get_global_data()->m_is_host == 2)
            {
                m_wifi_disconnect();
            }
            need_enter_ota = false;
            need_out_ota_prepare = true;
        }
    }
    //----------------------------尝试ota阶段-------------------------------//

    static OtaPrepareState* Instance()
    {
        static OtaPrepareState instance;
        return &instance;
    }
};
#endif