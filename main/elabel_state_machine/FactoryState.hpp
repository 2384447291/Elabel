// #ifndef FACTORYSTATE_HPP
// #define FACTORYSTATE_HPP

// #include "StateMachine.hpp"
// #include "ElabelController.hpp"
// #include "Codec.hpp"
// #include "control_driver.hpp"
// #include "battery_manager.hpp"

// #define Factory_coutdown 6


// typedef enum
// {
//     default_factory_process,
//     factory_button_process,
//     factory_speaker_process,
//     factory_mic_process,
//     factory_record_process,
//     factory_power_process,
//     factory_finish_process,
// } Factory_process;


// class FactoryState : public State<ElabelController>
// {
// private:

// public:
//     virtual void Init(ElabelController* pOwner);
//     virtual void Enter(ElabelController* pOwner);
//     virtual void Execute(ElabelController* pOwner);
//     virtual void Exit(ElabelController* pOwner);

//     Factory_process factory_process = default_factory_process;
//     bool Button_mask[8];
//     bool need_flash_paper = false;
//     uint8_t record_voice_countdown = Factory_coutdown;

//     static FactoryState* Instance()
//     {
//         static FactoryState instance;
//         return &instance;
//     }

//     void enter_button_process()
//     {
//         need_flash_paper = false;

//         for(int i = 0; i < 8; i++)
//         {
//             Button_mask[i] = false;
//         }
//         factory_process = factory_button_process;
//         lock_lvgl();
//         lv_obj_clear_flag(ui_ButtonPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_SpeakerPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_PowerPanel, LV_OBJ_FLAG_HIDDEN);
//         switch_screen(ui_FactoryScreen);
//         set_text_without_change_font(ui_ButtonCheck, "1  2  3  4\n5  6  7  8");
//         release_lvgl();
//     }


//     void enter_speaker_process()
//     {
//         need_flash_paper = false;
//         factory_process = factory_speaker_process;

//         lock_lvgl();
//         lv_obj_clear_flag(ui_SpeakerPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_ButtonPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_PowerPanel, LV_OBJ_FLAG_HIDDEN);
//         set_text_without_change_font(ui_CharacterName, "Speaker");

//         char volume_str[20];
//         sprintf(volume_str, "volume:%d", get_global_data()->m_device_info.sound_volume);
//         set_text_without_change_font(ui_CharacterDescribe, volume_str);
//         release_lvgl();
//         MCodec::Instance()->stop_play();
//         MCodec::Instance()->play_music("pikachu");
//     }

//     void enter_mic_process()
//     {
//         record_voice_countdown = Factory_coutdown;
//         need_flash_paper = false;
//         factory_process = factory_mic_process;

//         lock_lvgl();
//         lv_obj_clear_flag(ui_SpeakerPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_ButtonPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_PowerPanel, LV_OBJ_FLAG_HIDDEN);

//         lv_obj_clear_flag(ui_CharacterDescribe, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_RecordRetryDescribtion, LV_OBJ_FLAG_HIDDEN);
//         set_text_without_change_font(ui_CharacterName, "Microphone");
//         char time_str[20];
//         sprintf(time_str, "%d secs left", record_voice_countdown);
//         set_text_without_change_font(ui_CharacterDescribe, time_str);

//         release_lvgl();
//         vTaskDelay(500 / portTICK_PERIOD_MS);
//         MCodec::Instance()->stop_play();
//         MCodec::Instance()->start_record();
//     }

//     void enter_record_process()
//     {
//         need_flash_paper = false;
//         record_voice_countdown = Factory_coutdown;
        
//         factory_process = factory_record_process;
//         lock_lvgl();
//         lv_obj_clear_flag(ui_SpeakerPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_ButtonPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_PowerPanel, LV_OBJ_FLAG_HIDDEN);

//         lv_obj_add_flag(ui_CharacterDescribe, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_clear_flag(ui_RecordRetryDescribtion, LV_OBJ_FLAG_HIDDEN);
//         set_text_without_change_font(ui_CharacterName, "Microphone");
//         release_lvgl();

//         MCodec::Instance()->stop_play();
//         MCodec::Instance()->play_mic();
//     }

//     void enter_power_process()
//     {
//         need_flash_paper = false;

//         factory_process = factory_power_process;
//         lock_lvgl();
//         lv_obj_clear_flag(ui_PowerPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_ButtonPanel, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_flag(ui_SpeakerPanel, LV_OBJ_FLAG_HIDDEN);
//         float battery_level = BatteryManager::Instance()->getBatteryLevel();
//         bool is_usb_connected = BatteryManager::Instance()->is_usb_connected(battery_level);
//         set_text_without_change_font(ui_PowerName, "Power");
//         if(battery_level > 0.5f)
//         {
//             char battery_str[20];
//             sprintf(battery_str, "Battery:%.3fV", battery_level);
//             set_text_without_change_font(ui_BatteryDescribtion, battery_str);
//         }
//         else
//         {
//             set_text_without_change_font(ui_BatteryDescribtion, "Battery:Not detected");
//         }

//         if(is_usb_connected)
//         {
//             set_text_without_change_font(ui_WireDescribtion, "Wire:Detected");
//         }
//         else
//         {
//             set_text_without_change_font(ui_WireDescribtion, "Wire:Not detected");
//         }
//         release_lvgl();
//     }
// };

// #endif