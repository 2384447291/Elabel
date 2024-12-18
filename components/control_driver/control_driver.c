#include <stdio.h>
#include "control_driver.h"
#include "ws2812_control.h"
#include "led_strip.h"

#define TAG "CONTROL_DRIVER"

#define EC56_GPIO_A 36
#define EC56_GPIO_B 35
#define INCREMENT_THRESHOLD 2     // 连续增加/减少的阈值
#define TIME_WINDOW_MS 800       // 0.8秒的时间窗口

#define BUZZER_GPIO_PWM 1

#define BUTTON_GPIODOWM 20  //下面的按钮
#define BUTTON_GPIOUP 4     //下面的按钮
#define LONG_PRESS_TIME 800 // 长按时间阈值（毫秒）
#define DEBOUNCE_DELAY 20 // 防抖延迟（毫秒）

#define DEVICE_UPDATE_PERIOD 20


//------------------------------------------------触发注册处理------------------------------------------------//
// 定义各类回调函数数组
void (*button_down_long_press_call_back[CALL_BACK_MAX])();
void (*button_down_short_press_call_back[CALL_BACK_MAX])();
void (*button_up_long_press_call_back[CALL_BACK_MAX])();
void (*button_up_short_press_call_back[CALL_BACK_MAX])();
void (*encoder_right_circle_call_back[CALL_BACK_MAX])();
void (*encoder_left_circle_call_back[CALL_BACK_MAX])();

// 当前已注册的回调数量
uint8_t button_down_long_press_count = 0;
uint8_t button_down_short_press_count = 0;
uint8_t button_up_long_press_count = 0;
uint8_t button_up_short_press_count = 0;
uint8_t encoder_right_circle_count = 0;
uint8_t encoder_left_circle_count = 0;

/**
 * 通用回调注册函数
 */
void _register_callback(void (**callback_array)(), uint8_t *callback_count, void (*call_back)()) {
    if (!callback_array || !callback_count || !call_back) {
        ESP_LOGE("CALLBACK", "Invalid callback or array.");
        return;
    }

    // 检查回调函数数组是否已满
    if (*callback_count >= CALL_BACK_MAX) {
        ESP_LOGW("CALLBACK", "Callback array is full. Cannot register more callbacks.");
        return;
    }

    // 将回调函数添加到数组中
    callback_array[*callback_count] = call_back;
    (*callback_count)++;

    ESP_LOGI("CALLBACK", "Callback function registered successfully.");
}

/**
 * 通用回调删除函数
 */
void _unregister_callback(void (**callback_array)(), uint8_t *callback_count, void (*call_back)()) {
    if (!callback_array || !callback_count || !call_back) {
        ESP_LOGE("CALLBACK", "Invalid callback or array.");
        return;
    }

    // 查找回调函数在数组中的位置
    int index = -1;
    for (uint8_t i = 0; i < *callback_count; i++) {
        if (callback_array[i] == call_back) {
            index = i;
            break;
        }
    }

    // 如果未找到，直接返回
    if (index == -1) {
        ESP_LOGW("CALLBACK", "Callback function not found in array.");
        return;
    }

    // 删除回调函数并调整数组
    for (uint8_t i = index; i < *callback_count - 1; i++) {
        callback_array[i] = callback_array[i + 1];
    }

    // 清除最后一个元素，减少回调数量
    callback_array[*callback_count - 1] = NULL;
    (*callback_count)--;

    ESP_LOGI("CALLBACK", "Callback function unregistered successfully.");
}

/**
 * 触发回调函数数组
 */
void trigger_callbacks(void (**callback_array)(), uint8_t callback_count) {
    for (uint8_t i = 0; i < callback_count; i++) {
        if (callback_array[i]) {
            ESP_LOGI("CALLBACK", "%p",callback_array[i]);
            callback_array[i]();
        }
    }
}

/**
 * 注册按钮按下短按回调
 */
void register_button_down_short_press_call_back(void (*call_back)()) {
    _register_callback(button_down_short_press_call_back, &button_down_short_press_count, call_back);
}

/**
 * 删除按钮按下短按回调
 */
void unregister_button_down_short_press_call_back(void (*call_back)()) {
    _unregister_callback(button_down_short_press_call_back, &button_down_short_press_count, call_back);
}

/**
 * 注册按钮按下长按回调
 */
void register_button_down_long_press_call_back(void (*call_back)()) {
    _register_callback(button_down_long_press_call_back, &button_down_long_press_count, call_back);
}

/**
 * 删除按钮按下长按回调
 */
void unregister_button_down_long_press_call_back(void (*call_back)()) {
    _unregister_callback(button_down_long_press_call_back, &button_down_long_press_count, call_back);
}

/**
 * 注册按钮松开短按回调
 */
void register_button_up_short_press_call_back(void (*call_back)()) {
    _register_callback(button_up_short_press_call_back, &button_up_short_press_count, call_back);
}

/**
 * 删除按钮松开短按回调
 */
void unregister_button_up_short_press_call_back(void (*call_back)()) {
    _unregister_callback(button_up_short_press_call_back, &button_up_short_press_count, call_back);
}

/**
 * 注册按钮松开长按回调
 */
void register_button_up_long_press_call_back(void (*call_back)()) {
    _register_callback(button_up_long_press_call_back, &button_up_long_press_count, call_back);
}

/**
 * 删除按钮松开长按回调
 */
void unregister_button_up_long_press_call_back(void (*call_back)()) {
    _unregister_callback(button_up_long_press_call_back, &button_up_long_press_count, call_back);
}

/**
 * 注册编码器右旋回调
 */
void register_encoder_right_circle_call_back(void (*call_back)()) {
    _register_callback(encoder_right_circle_call_back, &encoder_right_circle_count, call_back);
}

/**
 * 删除编码器右旋回调
 */
void unregister_encoder_right_circle_call_back(void (*call_back)()) {
    _unregister_callback(encoder_right_circle_call_back, &encoder_right_circle_count, call_back);
}

/**
 * 注册编码器左旋回调
 */
void register_encoder_left_circle_call_back(void (*call_back)()) {
    _register_callback(encoder_left_circle_call_back, &encoder_left_circle_count, call_back);
}

/**
 * 删除编码器左旋回调
 */
void unregister_encoder_left_circle_call_back(void (*call_back)()) {
    _unregister_callback(encoder_left_circle_call_back, &encoder_left_circle_count, call_back);
}

//------------------------------------------------触发注册处理------------------------------------------------//


//------------------------------------------------LED逻辑处理------------------------------------------------//
ws2812_strip_t* elabel_led;
led_state elabel_led_state;

void set_led_state(led_state _led_state)
{
    elabel_led_state = _led_state;
}

void set_todo_time_up(uint32_t _time,uint32_t max_time)
{
    set_led_state(device_focus);
    if(_time > 4*max_time/5) led_set_pixel(elabel_led,4, (led_color_t){100, 0, 0});
    else led_set_pixel(elabel_led,4, (led_color_t){0, 0, 0});

    if(_time > 3*max_time/5) led_set_pixel(elabel_led,3, (led_color_t){100, 0, 0});
    else led_set_pixel(elabel_led,3, (led_color_t){0, 0, 0});

    if(_time > 2*max_time/5) led_set_pixel(elabel_led,2, (led_color_t){100, 0, 0});
    else led_set_pixel(elabel_led,2, (led_color_t){0, 0, 0});

    if(_time > max_time/5) led_set_pixel(elabel_led,1, (led_color_t){100, 0, 0});
    else led_set_pixel(elabel_led,1, (led_color_t){0, 0, 0});

    if(_time > 0) led_set_pixel(elabel_led,0, (led_color_t){100, 0, 0});
    else led_set_pixel(elabel_led,0, (led_color_t){0, 0, 0});
}

void led_update(void *Parameters) {
    while (1) 
    {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if(elabel_led_state == no_light)
        {
            led_set_off(elabel_led);
        }
        else if(elabel_led_state == device_init)
        {
            led_set_on(elabel_led, (led_color_t){0, 100, 0});
        }
        else if(elabel_led_state == device_no_wifi)
        {
            led_set_on(elabel_led, (led_color_t){100, 0, 0});
        }
        else if(elabel_led_state == device_wifi_connect)
        {
            led_set_blink_slow(elabel_led, (led_color_t){0, 0, 100}, 1);
        }
        else if(elabel_led_state == device_focus)
        {

        }
        else if(elabel_led_state == device_out_time)
        {
            led_set_blink_fast(elabel_led, (led_color_t){100, 0, 0}, 1);
        }
    }
}
//------------------------------------------------LED逻辑处理------------------------------------------------//


//------------------------------------------------button逻辑处理------------------------------------------------//
button_t button_down;
button_t button_up;

// 中断处理函数
static void IRAM_ATTR button_isr_handler(void *arg) {
    button_t *button = (button_t *)arg;
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    // 防抖逻辑
    if (current_time - button->last_interrupt_time > button->debounce_delay) {
        button->last_interrupt_time = current_time;
        // 标记按钮被按下
        button->button_pressed = true;
    }
}

void button_init(button_t *button, gpio_num_t button_gpio, const char *name, uint32_t debounce_delay, uint32_t long_press_time) {
    button->gpio_num = button_gpio;
    button->debounce_delay = debounce_delay;
    button->long_press_time = long_press_time;
    button->name = name;
    button->last_interrupt_time = 0;
    button->press_start_time = 0;
    button->button_pressed = false;
    button->button_released = true;

    // 配置GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,            // 下降沿触发
        .mode = GPIO_MODE_INPUT,                  // 输入模式
        .pin_bit_mask = (1ULL << button_gpio),       // 选择GPIO
        .pull_down_en = 0,                        // 禁用下拉
        .pull_up_en = GPIO_PULLUP_ENABLE          // 启用上拉
    };
    gpio_config(&io_conf);

    // 注册中断服务
    gpio_isr_handler_add(button_gpio, button_isr_handler, (void *)button);
}

void button_handle(button_t *button) {
    // 按钮释放
    if (gpio_get_level(button->gpio_num)) button->button_pressed = 0;
    
    // 按钮按下处理
    if (button->button_pressed && button->button_released) 
    {
        button->press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        button->button_released = false;
    }

    // 检测按钮松开
    if (!button->button_pressed && !button->button_released) { // 松开时
        uint32_t press_duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) - button->press_start_time;

        // 判断长按或短按
        if (press_duration >= button->long_press_time) 
        {
            if(button->gpio_num == BUTTON_GPIODOWM) trigger_callbacks(button_down_long_press_call_back, button_down_long_press_count);
            else if(button->gpio_num == BUTTON_GPIOUP) trigger_callbacks(button_up_long_press_call_back, button_up_long_press_count);
            ESP_LOGI(TAG, "%s long pressed\n", button->name);
        } 
        else 
        {
            if(button->gpio_num == BUTTON_GPIODOWM) trigger_callbacks(button_down_short_press_call_back, button_down_short_press_count);
            else if(button->gpio_num == BUTTON_GPIOUP) trigger_callbacks(button_up_short_press_call_back, button_up_short_press_count);
            ESP_LOGI(TAG, "%s short pressed\n", button->name);
        }
        button->button_released = true; // 重置状态
    }
}

void dealwithbutton()
{
    button_handle(&button_up);
    button_handle(&button_down);
}
//------------------------------------------------button逻辑处理------------------------------------------------//



//------------------------------------------------编码器逻辑处理------------------------------------------------//
uint8_t flag = 0;//0表示正转，1表示反转
uint8_t icnt = 0;//标志一个旋转任务的开始
int EncoderValue = 0;

int get_EncoderValue()
{
    return EncoderValue;
}

void reset_EncoderValue()
{
    EncoderValue = 0;
}

void set_EncoderValue(int value)
{
    EncoderValue = value;
}

int last_value = 0;
int increment_count = 0;
int decrement_count = 0;
int64_t start_time = 0;

void dealwithencoder(){
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
            ESP_LOGI(TAG, "Encoder right circle.\n");
            trigger_callbacks(encoder_right_circle_call_back, encoder_right_circle_count);
            increment_count = 0; // 重置计数，等待下一次触发
        } 
        else if (decrement_count >= INCREMENT_THRESHOLD) 
        {
            trigger_callbacks(encoder_left_circle_call_back, encoder_left_circle_count);
            ESP_LOGI(TAG, "Encoder left circle.\n");
            decrement_count = 0; // 重置计数，等待下一次触发事件位
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


//------------------------------------------------buzzer逻辑处理------------------------------------------------//
void buzzer_pwm_stop()
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void buzzer_pwm_start(uint32_t new_frequency)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 700);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, new_frequency);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

typedef enum {
    BUZZER_STATE_WAIT,
    BUZZER_STATE_OFF,
    BUZZER_STATE_ON,
    BUZZER_STATE_DELAY,
} buzzer_state_t;

buzzer_state_t buzzer_state = BUZZER_STATE_WAIT;

int buzzer_duration;
int buzzer_count;
int buzzer_delay;
int buzzer_time;
uint32_t buzzer_tick = 0;

uint32_t buzzer_start_tick = 0;
uint32_t buzzer_wait_tick = 0;
uint16_t temp_buzzer_count = 0;

bool need_reverse = false;

void update_buzzer_state(uint32_t Time_count_down ,float _duration, float _count, float _delay, float _buzzer_time)
{
    buzzer_tick = Time_count_down*1000;
    if(buzzer_tick == 0)need_reverse = true;
    else need_reverse = false;
    buzzer_duration = _duration*1000;
    buzzer_count = _count;
    buzzer_delay = _delay*1000;
    buzzer_time = _buzzer_time*1000;
    buzzer_state = BUZZER_STATE_WAIT;
    buzzer_pwm_stop();
}

void dealwithbuzzer()
{
    if(buzzer_state == BUZZER_STATE_WAIT)
    {
        //检测到使用了update_buzzer_state,刚设定不肯能叫
        if(buzzer_count!=0)
        {
            buzzer_state = BUZZER_STATE_OFF;
        }
        return; 
    }
    if(!need_reverse)
    {
        if(buzzer_state == BUZZER_STATE_OFF)
        {
            if(buzzer_tick%buzzer_duration==0)
            {
                buzzer_pwm_start(1000);
                buzzer_state = BUZZER_STATE_ON;
                buzzer_start_tick = buzzer_tick;
                temp_buzzer_count = buzzer_count;
            }
        }
        if(buzzer_state == BUZZER_STATE_ON)
        {
            if(buzzer_start_tick - buzzer_tick >= buzzer_time)
            {
                buzzer_pwm_stop();
                temp_buzzer_count --;
                if(temp_buzzer_count == 0)
                {
                    buzzer_state = BUZZER_STATE_OFF;
                }
                else
                {
                    buzzer_state = BUZZER_STATE_DELAY;
                    buzzer_wait_tick = buzzer_tick;
                }
            }
        }
        if(buzzer_state == BUZZER_STATE_DELAY)
        {
            if(buzzer_wait_tick - buzzer_tick >= buzzer_delay)
            {
                buzzer_pwm_start(1000);
                buzzer_state = BUZZER_STATE_ON;
                buzzer_start_tick = buzzer_tick;
            }
        }
        buzzer_tick-=DEVICE_UPDATE_PERIOD;
    }
    else
    {
        if(buzzer_state == BUZZER_STATE_OFF)
        {
            if(buzzer_tick%buzzer_duration == 0)
            {
                buzzer_pwm_start(1000);
                buzzer_start_tick = buzzer_tick;
                buzzer_state = BUZZER_STATE_ON;
            }
        }
        
        if(buzzer_state == BUZZER_STATE_ON)
        {
            if(buzzer_tick - buzzer_start_tick >= buzzer_time)
            {
                buzzer_pwm_stop();
                buzzer_state = BUZZER_STATE_OFF;
            }
        }

        buzzer_tick+=DEVICE_UPDATE_PERIOD;
    }
}
//------------------------------------------------buzzer逻辑处理------------------------------------------------//


void control_panel_update(void *Parameters) {
    while (1) {
        vTaskDelay(DEVICE_UPDATE_PERIOD / portTICK_PERIOD_MS);
        dealwithbutton();
        dealwithencoder();
        dealwithbuzzer();
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) 
{
    uint32_t gpio_num = (uint32_t)arg;
    //-------------------------------------编码器处理-------------------------------------//
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
    //-------------------------------------编码器处理-------------------------------------//
}

void control_gpio_init(void)
{
    gpio_install_isr_service(0);
    //------------------------------------------------gpio初始化------------------------------------------------//
    button_init(&button_up, BUTTON_GPIOUP, "Button_UP", DEBOUNCE_DELAY, LONG_PRESS_TIME);
    button_init(&button_down, BUTTON_GPIODOWM, "Button_DOWM", DEBOUNCE_DELAY, LONG_PRESS_TIME);
    //------------------------------------------------gpio初始化------------------------------------------------//


    //------------------------------------------------encoder初始化------------------------------------------------//
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

    gpio_isr_handler_add(EC56_GPIO_A, gpio_isr_handler, (void*)EC56_GPIO_A);
    //------------------------------------------------encoder初始化------------------------------------------------//


    //------------------------------------------------PWM_Buzzer初始化------------------------------------------------//
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2700,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = BUZZER_GPIO_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
    buzzer_pwm_stop();
    //------------------------------------------------PWM_Buzzer初始化------------------------------------------------//


    //------------------------------------------------LED初始化------------------------------------------------//
    elabel_led=ws2812_create();
    //------------------------------------------------LED初始化------------------------------------------------//

    //开始检测触发线程
    xTaskCreate(control_panel_update, "control_panel_updat", 3072, NULL, 10, NULL);
    xTaskCreate(led_update, "led_update", 2048, NULL, 10, NULL);
}
