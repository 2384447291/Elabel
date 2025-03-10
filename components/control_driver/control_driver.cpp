#include "control_driver.hpp"

#define TAG "CONTROL_DRIVER"


// ControlDriver类实现
void ControlDriver::init() {
    // 注册按钮
    Button::register_single_io(DEVICE_BUTTON_1, &button1);
    Button::register_single_io(DEVICE_BUTTON_2, &button2);
    Button::register_share_io_2(DEVICE_BUTTON_34, DEVICE_BUTTON_34_CHANNEL, &button4, &button3);
    Button::register_share_io_2(DEVICE_BUTTON_56, DEVICE_BUTTON_56_CHANNEL, &button5, &button6);
    Button::register_share_io_2(DEVICE_BUTTON_78, DEVICE_BUTTON_78_CHANNEL, &button8, &button7);
    // 创建任务
    xTaskCreate(controlPanelUpdateTask, "control_panel_update", 4096, nullptr, 10, nullptr);
}

void ControlDriver::controlPanelUpdateTask(void* parameters) {
    while (true) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        
        // 处理按钮
        ControlDriver::Instance()->button1.handle();
        ControlDriver::Instance()->button2.handle();
        ControlDriver::Instance()->button3.handle();
        ControlDriver::Instance()->button4.handle();
        ControlDriver::Instance()->button5.handle();    
        ControlDriver::Instance()->button6.handle();
        ControlDriver::Instance()->button7.handle();
        ControlDriver::Instance()->button8.handle();
    }
}
