#include <stdio.h>
#include "control_driver.h"

#define CONTROL_DRIVER "CONTROL_DRIVER"

#define EC56_GPIO_A 21
#define EC56_GPIO_B 47
#define BUTTON_GPIO1 2
#define LONG_PRESS_TIME 1000 // 长按时间阈值（毫秒）
#define DEBOUNCE_DELAY 20 // 防抖延迟（毫秒）

#define INCREMENT_THRESHOLD 3     // 连续增加/减少的阈值
#define TIME_WINDOW_MS 1200       // 1秒的时间窗口

//------------------------------------------------button逻辑处理------------------------------------------------//
button_interrupt m_button_interrupt;
button_interrupt get_button_interrupt(void) 
{
    return m_button_interrupt;
}

uint8_t pressed = 0;
uint8_t get_pressed()
{
    return pressed;
}

uint32_t press_start_time = 0; // 按下开始时间
bool button_released = true; // 用于记录按钮释放状态
void dealwithbutton()
{
    // 按钮释放
    if (gpio_get_level(BUTTON_GPIO1)) pressed = 0;

    // 按钮按下处理
    if (pressed && button_released) {
        // 第一次按下时记录按下时间
        press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        button_released = false;
    }

    // 按钮松开时的长短按判断
    if (!pressed && !button_released) {
        uint32_t press_duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) - press_start_time;

        if (press_duration >= LONG_PRESS_TIME) {
            // 长按
            ESP_LOGI(CONTROL_DRIVER, "Button long pressed\n");
            m_button_interrupt = long_press;
        } else {
            // 短按
            ESP_LOGI(CONTROL_DRIVER, "Button short pressed\n");
            m_button_interrupt = short_press;
        }

        xEventGroupSetBits(get_eventgroupe(), BUTTON_TASK_BIT); // 设置事件位
        button_released = true; // 重置状态，等待下次按下
    }
}
//------------------------------------------------button逻辑处理------------------------------------------------//



//------------------------------------------------编码器逻辑处理------------------------------------------------//
uint8_t flag = 0;//0表示正转，1表示反转
uint8_t icnt = 0;//标志一个旋转任务的开始
int EncoderValue = 0;
encoder_interrupt m_encoder_interrupt;
encoder_interrupt get_encoder_interrupt(void) 
{
    return m_encoder_interrupt;
}

int get_EncoderValue()
{
    return EncoderValue;
}

void reset_EncoderValue()
{
    EncoderValue = 0;
}

int last_value = 0;
int increment_count = 0;
int decrement_count = 0;
int64_t start_time = 0;

void dealwithencoder()
{
    int current_value = EncoderValue;
    int64_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS; // 当前时间（毫秒）

    // 检测正转和反转
    if (current_value > last_value) {
        // 如果检测到新的增加趋势，则从这一刻开始计时
        if (increment_count == 0 || decrement_count > 0) {
            start_time = current_time; // 记录开始时间
            increment_count = 0;
            decrement_count = 0; // 重置反转计数
        }
        increment_count += (current_value - last_value);
    } else if (current_value < last_value) {
        // 如果检测到新的减少趋势，则从这一刻开始计时
        if (decrement_count == 0 || increment_count > 0) {
            start_time = current_time; // 记录开始时间
            increment_count = 0;       // 重置正转计数
            decrement_count = 0;
        }
        decrement_count += (last_value - current_value);
    }

    last_value = current_value;

    // 检查是否在时间窗口内超过阈值
    if (current_time - start_time <= TIME_WINDOW_MS) 
    {
        if (increment_count >= INCREMENT_THRESHOLD) 
        {
            ESP_LOGI(CONTROL_DRIVER, "Encoder right circle.\n");
            m_encoder_interrupt = right_circle;
            increment_count = 0; // 重置计数，等待下一次触发
            xEventGroupSetBits(get_eventgroupe(), ENCODER_TASK_BIT); // 设置事件位
        } 
        else if (decrement_count >= INCREMENT_THRESHOLD) 
        {
            ESP_LOGI(CONTROL_DRIVER, "Encoder left circle.\n");
            m_encoder_interrupt = left_circle;
            decrement_count = 0; // 重置计数，等待下一次触发
            xEventGroupSetBits(get_eventgroupe(), ENCODER_TASK_BIT); // 设置事件位
        } 
    }
    else
    {
        // 重置计数和时间
        increment_count = 0;
        decrement_count = 0;
    }
}
//------------------------------------------------编码器逻辑处理------------------------------------------------//

void control_panel_update(void *Parameters) {
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        dealwithbutton();
        dealwithencoder();
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;

    //编码器处理
    if(gpio_num == EC56_GPIO_A)
    {
        //如果是A是上升沿，检测B如果为高电平，则为正转+1
        if(gpio_get_level(EC56_GPIO_A) && icnt == 0)
        {
            if(gpio_get_level(EC56_GPIO_B)) flag = 1;
            else flag = 0;
            icnt = 1;
        } 
        if(icnt && !gpio_get_level(EC56_GPIO_A))
        {
            if(gpio_get_level(EC56_GPIO_B) == 0 && flag == 1)
            {
                EncoderValue++;
            }
            if(gpio_get_level(EC56_GPIO_B) == 1 && flag == 0)
            {
                EncoderValue--;
            }
            icnt = 0;//结束该次旋转任务的开始
        }
    }

    // 按钮处理（防抖）
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (current_time - last_interrupt_time > DEBOUNCE_DELAY) {
        last_interrupt_time = current_time; // 更新最后中断时间
        if (gpio_num == BUTTON_GPIO1) {
                pressed = 1;
        }
    }
}

void control_gpio_init(void)
{
    gpio_config_t io_conf = {};
	// A 相 IO 配置为中断上升沿/下降沿都触发的模式
    io_conf.intr_type = GPIO_INTR_ANYEDGE; /* 上升沿/下降沿都触发 */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL<<EC56_GPIO_A; 
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

	// B 相 IO 配置为普通的输入模式
    io_conf.intr_type = GPIO_INTR_DISABLE; /* 禁用中断 */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL<<EC56_GPIO_B; 
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    //按键中断
    io_conf.intr_type = GPIO_INTR_NEGEDGE;/* 下降沿都触发 */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL<<BUTTON_GPIO1);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(EC56_GPIO_A, gpio_isr_handler, (void*)EC56_GPIO_A);
    gpio_isr_handler_add(BUTTON_GPIO1, gpio_isr_handler, (void*)BUTTON_GPIO1);
    xTaskCreate(control_panel_update, "control_panel_updat", 2048, NULL, 10, NULL);
}
