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
    // 配置 ADC oneshot
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    xadcSemaphore = xSemaphoreCreateMutex();

#ifdef R01A_TEST
    button_pair_1234.init(adc_handle, DEVICE_BUTTON_1234_CHANNEL, &button1, &button2, &button3, &button4);
    button_pair_567.init(adc_handle, DEVICE_BUTTON_567_CHANNEL, &button5, &button6, &button7);
    button_pair_8.init(adc_handle, DEVICE_BUTTON_8_CHANNEL, &button8);
#elif defined(R01B_TEST)
    button_pair_1234.init(adc_handle, DEVICE_BUTTON_1234_CHANNEL, &button1, &button2, &button3, &button4);
    button_pair_5678.init(adc_handle, DEVICE_BUTTON_5678_CHANNEL, &button5, &button6, &button7, &button8);
#endif

   start_button_check_task();
}

void ControlDriver::start_button_check_task() 
{
    if (button_check_task_handle == nullptr) {
        button_pair_1234.clear_state();
#ifdef R01A_TEST
        button_pair_567.clear_state();
        button_pair_8.clear_state();
#elif defined(R01B_TEST)
        button_pair_5678.clear_state();
#endif
        button_press_together_48.clear_state();
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
        for(int i = 0; i < 4; i++) {
            ControlDriver::Instance()->button_pair_1234.button[i]->handle();
        }

#ifdef R01A_TEST
        ControlDriver::Instance()->button_pair_567.update();
        ControlDriver::Instance()->button_pair_8.update();
#elif defined(R01B_TEST)
        ControlDriver::Instance()->button_pair_5678.update();
#endif


#ifdef R01A_TEST
        for(int i = 0; i < 3; i++) {
            ControlDriver::Instance()->button_pair_567.button[i]->handle();
        }
        ControlDriver::Instance()->button_pair_8.button->handle();
#elif defined(R01B_TEST)
        for(int i = 0; i < 4; i++) {
            ControlDriver::Instance()->button_pair_5678.button[i]->handle();
        }
#endif
        ControlDriver::Instance()->button_press_together_48.update();
        ControlDriver::Instance()->button_press_together_15.update();
    }
}

