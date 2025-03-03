#include "control_driver.hpp"

#define TAG "CONTROL_DRIVER"


// ControlDriver类实现
void ControlDriver::init() {
    // 注册按钮
    Button::register_single_io(DEVICE_BUTTON_1, &button1);
    Button::register_share_io_2(DEVICE_BUTTON_34, DEVICE_BUTTON_34_CHANNEL, &button3, &button4);
    // 创建任务
    xTaskCreate(controlPanelUpdateTask, "control_panel_update", 3072, nullptr, 10, nullptr);
}

void ControlDriver::controlPanelUpdateTask(void* parameters) {
    while (true) {
        vTaskDelay(DEVICE_UPDATE_PERIOD / portTICK_PERIOD_MS);
        
        // 处理按钮
        ControlDriver::Instance()->button1.handle();
        ControlDriver::Instance()->button2.handle();
        ControlDriver::Instance()->button3.handle();
        ControlDriver::Instance()->button4.handle();
        ControlDriver::Instance()->button5.handle();    
        ControlDriver::Instance()->button6.handle();
        ControlDriver::Instance()->button7.handle();
        ControlDriver::Instance()->button8.handle();
        // 处理蜂鸣器
        ControlDriver::Instance()->buzzer.handle();
    }
}
