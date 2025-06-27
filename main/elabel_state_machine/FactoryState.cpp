#include "FactoryState.hpp"

void Reveal_mask(int mask_index)
{
    if(FactoryState::Instance()->factory_process != factory_button_process) return;
    if(!FactoryState::Instance()->Button_mask[mask_index])
    {
        FactoryState::Instance()->Button_mask[mask_index] = true;
        FactoryState::Instance()->need_flash_paper = true;
    }
}

void Reveal_mask_1() {Reveal_mask(0);}
void Reveal_mask_2() {Reveal_mask(1);}
void Reveal_mask_3() {Reveal_mask(2);}
void Reveal_mask_4() {Reveal_mask(3);}
void Reveal_mask_5() {Reveal_mask(4);}
void Reveal_mask_6() {Reveal_mask(5);}
void Reveal_mask_7() {Reveal_mask(6);}
void Reveal_mask_8() {Reveal_mask(7);}

void Add_speaker_volume()
{
    if(FactoryState::Instance()->factory_process != factory_speaker_process) return;
    uint8_t current_volume = get_global_data()->m_device_info.sound_volume;
    current_volume += 5;
    if(current_volume > 100) current_volume = 100;
    get_global_data()->m_device_info.sound_volume = current_volume;
    MCodec::Instance()->set_speaker_volume(current_volume);
    FactoryState::Instance()->need_flash_paper = true;
}

void Sub_speaker_volume()
{
    if(FactoryState::Instance()->factory_process != factory_speaker_process) return;
    uint8_t current_volume = get_global_data()->m_device_info.sound_volume;
    current_volume -= 5;
    if(current_volume < 0) current_volume = 0;
    get_global_data()->m_device_info.sound_volume = current_volume;
    MCodec::Instance()->set_speaker_volume(current_volume);
    FactoryState::Instance()->need_flash_paper = true;
}

void Enter_next_process()
{
    if(FactoryState::Instance()->factory_process == factory_speaker_process)
    {
        MCodec::Instance()->stop_play();
        FactoryState::Instance()->enter_mic_process();
    }
    else if(FactoryState::Instance()->factory_process == factory_record_process)
    {
        MCodec::Instance()->stop_play();
        FactoryState::Instance()->enter_power_process();
    }
    else if(FactoryState::Instance()->factory_process == factory_power_process)
    {
        FactoryState::Instance()->factory_process = factory_finish_process;
        esp_restart();
    }
}

void record_voice_again()
{
    if(FactoryState::Instance()->factory_process == factory_record_process)
    {
        MCodec::Instance()->stop_play();
        FactoryState::Instance()->enter_mic_process();        
    }
}

void FactoryState::Init(ElabelController* pOwner){}

void FactoryState::Enter(ElabelController* pOwner)
{
    FactoryState::Instance()->factory_process = default_factory_process;
    for(int i = 0; i < 8; i++)
    {
        FactoryState::Instance()->Button_mask[i] = false;
    }
    FactoryState::Instance()->need_flash_paper = false;
    FactoryState::Instance()->record_voice_countdown = Factory_coutdown;

    ESP_LOGI(STATEMACHINE,"Enter FactoryState.");

    enter_button_process();

    ControlDriver::Instance()->button1.CallbackShortPress.registerCallback(Reveal_mask_1);
    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(Reveal_mask_2);
    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(Reveal_mask_3);
    ControlDriver::Instance()->button4.CallbackShortPress.registerCallback(Reveal_mask_4);
    ControlDriver::Instance()->button5.CallbackShortPress.registerCallback(Reveal_mask_5);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(Reveal_mask_6);
    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(Reveal_mask_7);
    ControlDriver::Instance()->button8.CallbackShortPress.registerCallback(Reveal_mask_8);

    ControlDriver::Instance()->button3.CallbackShortPress.registerCallback(Enter_next_process);
    ControlDriver::Instance()->button2.CallbackShortPress.registerCallback(record_voice_again);

    ControlDriver::Instance()->button7.CallbackShortPress.registerCallback(Add_speaker_volume);
    ControlDriver::Instance()->button6.CallbackShortPress.registerCallback(Sub_speaker_volume);
}   

void FactoryState::Execute(ElabelController* pOwner)
{
    if(factory_process == factory_button_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            bool is_all_mask_revealed = true;
            for(int i = 0; i < 8; i++)
            {
                //如果没有被按下
                if(!Button_mask[i])
                {
                    is_all_mask_revealed = false;
                    lv_obj_add_flag(Button_mask_ui[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_clear_flag(Button_mask_ui[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            release_lvgl();
            if(is_all_mask_revealed) enter_speaker_process();
            need_flash_paper = false;
        }
    }
    else if(factory_process == factory_speaker_process)
    {
        if(need_flash_paper)
        {
            lock_lvgl();
            char volume_str[20];
            sprintf(volume_str, "volume:%d%%", get_global_data()->m_device_info.sound_volume);
            set_text_without_change_font(ui_CharacterDescribe, volume_str);
            release_lvgl();
            need_flash_paper = false;
            MCodec::Instance()->stop_play();
            MCodec::Instance()->play_music("pikachu");
        }
    }
    else if(factory_process == factory_mic_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            if(record_voice_countdown > 0)
            {
                record_voice_countdown--;
                lock_lvgl();
                char time_str[20];
                sprintf(time_str, "%d secs left", record_voice_countdown);
                set_text_without_change_font(ui_CharacterDescribe, time_str);
                release_lvgl();
            }
        }
        if(MCodec::Instance()->mic_task == NULL && record_voice_countdown == 0)
        {
            enter_record_process();
        }
    }
    else if(factory_process == factory_power_process)
    {
        if(elabelUpdateTick%1000 == 0)
        {
            lock_lvgl();
            float battery_level = BatteryManager::Instance()->getBatteryLevel();
            bool is_usb_connected = BatteryManager::Instance()->is_usb_connected(battery_level);
            set_text_without_change_font(ui_PowerName, "Power");
            if(battery_level > 0.5f)
            {
                char battery_str[20];
                sprintf(battery_str, "Battery:%.3fV", battery_level);
                set_text_without_change_font(ui_BatteryDescribtion, battery_str);
            }
            else
            {
                set_text_without_change_font(ui_BatteryDescribtion, "Battery:Not detected");
            }

            if(is_usb_connected)
            {
                set_text_without_change_font(ui_WireDescribtion, "Wire:Detected");
            }
            else
            {
                set_text_without_change_font(ui_WireDescribtion, "Wire:Not detected");
            }
            release_lvgl();
        }        
    }
}

void FactoryState::Exit(ElabelController* pOwner)
{
    ControlDriver::Instance()->button1.CallbackShortPress.unregisterCallback(Reveal_mask_1);
    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(Reveal_mask_2);
    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(Reveal_mask_3);
    ControlDriver::Instance()->button4.CallbackShortPress.unregisterCallback(Reveal_mask_4);
    ControlDriver::Instance()->button5.CallbackShortPress.unregisterCallback(Reveal_mask_5);
    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(Reveal_mask_6);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(Reveal_mask_7);
    ControlDriver::Instance()->button8.CallbackShortPress.unregisterCallback(Reveal_mask_8);

    ControlDriver::Instance()->button3.CallbackShortPress.unregisterCallback(Enter_next_process);
    ControlDriver::Instance()->button2.CallbackShortPress.unregisterCallback(record_voice_again);

    ControlDriver::Instance()->button6.CallbackShortPress.unregisterCallback(Add_speaker_volume);
    ControlDriver::Instance()->button7.CallbackShortPress.unregisterCallback(Sub_speaker_volume);

}