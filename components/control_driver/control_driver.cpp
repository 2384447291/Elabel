#include "control_driver.hpp"

#define TAG "CONTROL_DRIVER"

// Button类实现
Button::Button(gpio_num_t gpio, const char* buttonName, uint32_t debounceDelay, uint32_t longPressTime)
    : gpioNum(gpio), 
      name(buttonName),
      debounceDelay(debounceDelay),
      longPressTime(longPressTime),
      state(State::IDLE),
      pressTime(0),
      lastPressTime(0),
      clickCount(0),
      isrTriggered(false) {
    
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << gpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio, buttonIsrHandler, this);
}

void IRAM_ATTR Button::buttonIsrHandler(void* arg) {
    Button* button = static_cast<Button*>(arg);
    button->isrTriggered = true;
}

void Button::handle() {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool buttonLevel = gpio_get_level(gpioNum);

    switch (state) {
        case State::IDLE:
            if (isrTriggered && !buttonLevel) {  // 按钮被按下
                pressTime = currentTime;
                state = State::PRESSED;
                isrTriggered = false;
            }
            break;

        case State::PRESSED:
            if (!buttonLevel) {  // 按钮仍然按下
                if (currentTime - pressTime >= longPressTime) {
                    // 触发长按
                    if (gpioNum == DEVICE_DOWN_BUTTON) {
                        ControlDriver::Instance()->ButtonDownLongPress.trigger();
                    } else if (gpioNum == DEVICE_UP_BUTTON) {
                        ControlDriver::Instance()->ButtonUpLongPress.trigger();
                    } else if (gpioNum == DEVICE_POWER_BUTTON) {
                        ControlDriver::Instance()->ButtonPowerLongPress.trigger();
                    }
                    ESP_LOGI(TAG, "%s long pressed", name);
                    state = State::WAIT_RELEASE;
                }
            } else {  // 按钮释放
                if (currentTime - lastPressTime < 300) {  // 300ms内的第二次按下
                    clickCount = 2;  // 确认是双击
                    if (gpioNum == DEVICE_DOWN_BUTTON) {
                        ControlDriver::Instance()->ButtonDownDoubleclick.trigger();
                    } else if (gpioNum == DEVICE_UP_BUTTON) {
                        ControlDriver::Instance()->ButtonUpDoubleclick.trigger();
                    } else if (gpioNum == DEVICE_POWER_BUTTON) {
                        ControlDriver::Instance()->ButtonPowerDoubleclick.trigger();
                    }
                    ESP_LOGI(TAG, "%s double clicked", name);
                    state = State::IDLE;
                } else {
                    // 第一次按下，等待可能的第二次按下
                    lastPressTime = currentTime;
                    clickCount = 1;
                    state = State::WAIT_DOUBLE_CLICK;
                }
            }
            break;

        case State::WAIT_RELEASE:
            if (buttonLevel) {  // 按钮释放
                state = State::IDLE;
                clickCount = 0;
            }
            break;

        case State::WAIT_DOUBLE_CLICK:
            if (!buttonLevel) {  // 在等待期间又按下了
                state = State::PRESSED;
            } else if (currentTime - lastPressTime >= 300) {  // 超过300ms没有第二次按下
                // 触发短按
                if (gpioNum == DEVICE_DOWN_BUTTON) {
                    ControlDriver::Instance()->ButtonDownShortPress.trigger();
                } else if (gpioNum == DEVICE_UP_BUTTON) {
                    ControlDriver::Instance()->ButtonUpShortPress.trigger();
                } else if (gpioNum == DEVICE_POWER_BUTTON) {
                    ControlDriver::Instance()->ButtonPowerShortPress.trigger();
                }
                ESP_LOGI(TAG, "%s short pressed", name);
                state = State::IDLE;
            }
            break;
    }
}

// Encoder类实现
Encoder::Encoder() 
    : value(0), aTriggered(false), lastB(0), lastRotationTime(0), rotationCount(0) {
    
    // 配置A引脚（中断引脚）
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << EC56_GPIO_A;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // 配置B引脚（状态读取引脚）
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ULL << EC56_GPIO_B;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(EC56_GPIO_A, encoderIsrHandler, this);
}

void IRAM_ATTR Encoder::encoderIsrHandler(void* arg) {
    Encoder* encoder = static_cast<Encoder*>(arg);
    bool aLevel = gpio_get_level(EC56_GPIO_A);
    
    if (aLevel) {  // A引脚上升沿
        encoder->lastB = gpio_get_level(EC56_GPIO_B);
        encoder->aTriggered = true;
    } else if (encoder->aTriggered) {  // A引脚下降沿且之前有上升沿
        bool bLevel = gpio_get_level(EC56_GPIO_B);
        if (bLevel == 0 && encoder->lastB == 1) {
            encoder->value++;  // 顺时针
        } else if (bLevel == 1 && encoder->lastB == 0) {
            encoder->value--;  // 逆时针
        }
        encoder->aTriggered = false;
    }
}

void Encoder::handle() {
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    int currentValue = value;
    static int lastValue = currentValue;
    
    // 检测旋转方向和计数
    if (currentValue != lastValue) {
        if (currentTime - lastRotationTime > ROTATION_TIMEOUT) {
            rotationCount = 0;
        }
        
        if (currentValue > lastValue) {
            rotationCount++;
            if (rotationCount >= ROTATION_THRESHOLD) {
                ControlDriver::Instance()->EncoderRightCircle.trigger();
                ESP_LOGI(TAG, "Encoder right circle");
                rotationCount = 0;
            }
        } else {
            rotationCount--;
            if (rotationCount <= -ROTATION_THRESHOLD) {
                ControlDriver::Instance()->EncoderLeftCircle.trigger();
                ESP_LOGI(TAG, "Encoder left circle");
                rotationCount = 0;
            }
        }
        
        lastRotationTime = currentTime;
        lastValue = currentValue;
    }
}

// ControlDriver类实现
void ControlDriver::init() {
    gpio_install_isr_service(0);

    // 初始化蜂鸣器
    buzzer.init();

    led.init();

    // 创建任务
    xTaskCreate(controlPanelUpdateTask, "control_panel_update", 3072, nullptr, 10, nullptr);
}

void ControlDriver::controlPanelUpdateTask(void* parameters) {
    while (true) {
        vTaskDelay(DEVICE_UPDATE_PERIOD / portTICK_PERIOD_MS);
        
        // 处理按钮
        ControlDriver::Instance()->buttonDown.handle();
        ControlDriver::Instance()->buttonUp.handle();
        ControlDriver::Instance()->buttonPower.handle();
        
        // 处理编码器
        ControlDriver::Instance()->encoder.handle();

        // 处理蜂鸣器
        ControlDriver::Instance()->buzzer.handle();

        // 处理LED
        ControlDriver::Instance()->led.handle();
    }
}
