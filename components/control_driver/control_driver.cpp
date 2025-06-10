#include "control_driver.hpp"

#define TAG "CONTROL_DRIVER"

void ControlDriver::register_all_button_callback(Callback::CallbackFunc callback)
{
    button1.CallbackShortPress.registerCallback(callback);
    button2.CallbackShortPress.registerCallback(callback);
    button3.CallbackShortPress.registerCallback(callback);
    button4.CallbackShortPress.registerCallback(callback);
    button5.CallbackShortPress.registerCallback(callback);
    button6.CallbackShortPress.registerCallback(callback);
    button7.CallbackShortPress.registerCallback(callback);
    button8.CallbackShortPress.registerCallback(callback);
}

void ControlDriver::unregister_button_callback(Callback::CallbackFunc callback)
{
    button1.CallbackShortPress.unregisterCallback(callback);
    button2.CallbackShortPress.unregisterCallback(callback);
    button3.CallbackShortPress.unregisterCallback(callback);
    button4.CallbackShortPress.unregisterCallback(callback);
    button5.CallbackShortPress.unregisterCallback(callback);
    button6.CallbackShortPress.unregisterCallback(callback);
    button7.CallbackShortPress.unregisterCallback(callback);
    button8.CallbackLongPress.unregisterCallback(callback);
}

// ControlDriver类实现
void ControlDriver::init() 
{
   start_button_check_task();
}

void ControlDriver::start_button_check_task() 
{
    if (button_check_task_handle == nullptr) {
        button_pair_1234.clear_state();
        button_pair_567.clear_state();
        button_pair_8.clear_state();
        button_press_together_58.clear_state();
        button_press_together_15.clear_state();
        xTaskCreate(button_check_task, "button_check_task", 4096, nullptr, 10, &button_check_task_handle);
    }
    else {
        ESP_LOGE(TAG, "button_check_task already exists");
    }
}

void ControlDriver::stop_button_check_task() {
    if (button_check_task_handle != nullptr) {
        vTaskDelete(button_check_task_handle);
        button_check_task_handle = nullptr;
    }
    else {
        ESP_LOGE(TAG, "button_check_task not exists");
    }
}

void ControlDriver::button_check_task(void* parameters) {
    while (true) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        ControlDriver::Instance()->button_pair_1234.update();
        ControlDriver::Instance()->button_pair_567.update();
        ControlDriver::Instance()->button_pair_8.update();
        ControlDriver::Instance()->button_press_together_58.update();
        ControlDriver::Instance()->button_press_together_15.update();
        for(int i = 0; i < 4; i++) {
            ControlDriver::Instance()->button_pair_1234.button[i]->handle();
        }
        for(int i = 0; i < 3; i++) {
            ControlDriver::Instance()->button_pair_567.button[i]->handle();
        }
        ControlDriver::Instance()->button_pair_8.button->handle();
    }
}

