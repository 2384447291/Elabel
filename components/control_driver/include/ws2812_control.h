/**
 * @file ws2812_control.h
 * @author 宁子希 (1589326497@qq.com)
 * @brief    WS2812灯条和矩阵屏幕控制 依赖led_strip库
 * @version 1.2.0
 * @date 2024-08-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef WS2812_CONTROL_H
#define WS2812_CONTROL_H
#define CONFIG_WS2812_MODE_STRIP
#define CONFIG_WS2812_TX_GPIO 45
#define CONFIG_WS2812_STRIP_LED_NUMBER 5

#include <stdint.h>
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef led_strip_t ws2812_strip_t;

// 定义颜色宏 
#define COLOR_RGB(r, g, b) ((led_color_t){(r), (g), (b)})   //自定义RGB颜色
#define COLOR_WHITE  (led_color_t){255, 255, 255}  // 白色
#define COLOR_GREEN  (led_color_t){0, 255, 0}      // 绿色
#define COLOR_RED    (led_color_t){255, 0, 0}      // 红色
#define COLOR_BLUE   (led_color_t){0, 0, 255}      // 蓝色
#define COLOR_YELLOW (led_color_t){255, 255, 0}    // 黄色
#define COLOR_CYAN   (led_color_t){0, 255, 255}    // 青色
#define COLOR_MAGENTA (led_color_t){255, 0, 255}   // 洋红色
#define COLOR_BLACK  (led_color_t){0, 0, 0}        // 黑色
#define COLOR_ORANGE (led_color_t){255, 165, 0}    // 橙色
#define COLOR_PURPLE (led_color_t){128, 0, 128}    // 紫色
#define COLOR_PINK   (led_color_t){255, 192, 203}  // 粉色
#define COLOR_GRAY   (led_color_t){128, 128, 128}  // 灰色
#define COLOR_BROWN  (led_color_t){165, 42, 42}    // 棕色
#define COLOR_LIME   (led_color_t){0, 255, 0}      // 酸橙绿
#define COLOR_TEAL   (led_color_t){0, 128, 128}    // 蓝绿色
#define COLOR_NAVY   (led_color_t){0, 0, 128}      // 海军蓝
#define COLOR_FUCHSIA (led_color_t){255, 0, 255}   // 紫红色
#define COLOR_MAROON (led_color_t){128, 0, 0}      // 栗色
#define COLOR_OLIVE  (led_color_t){128, 128, 0}    // 橄榄绿

// led_color_t 结构体表示 LED 灯的颜色
typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} led_color_t;

//ws2812 灯带模式结构体定义
#ifdef CONFIG_WS2812_MODE_STRIP




// led_effect_t 枚举类型表示 LED 灯的效果类型
typedef enum {
    // 关闭LED灯效果
    LED_EFFECT_OFF,
    // 使LED灯常亮效果
    LED_EFFECT_ON,
    // 使LED灯呼吸效果
    LED_EFFECT_BREATH,
    // 使LED灯淡入效果
    LED_EFFECT_FADE_IN,
    // 使LED灯慢闪烁效果
    LED_EFFECT_BLINK_SLOW,
    // 使LED灯快闪烁效果
    LED_EFFECT_BLINK_FAST,
    // 使LED灯彩虹效果
    LED_EFFECT_RAINBOW
} led_effect_t;

//ws2812 矩阵屏幕模式结构体定义
#elif defined(CONFIG_WS2812_MODE_MATRIX)

/**
 * @brief 矩阵屏幕类型，用于定义矩阵屏幕的布局和扫描方式
 * 
 */
#define MATRIX_START_TOP_LEFT     0x00  // 左上角
#define MATRIX_START_TOP_RIGHT    0x01  // 右上角
#define MATRIX_START_BOTTOM_LEFT  0x02  // 左下角
#define MATRIX_START_BOTTOM_RIGHT 0x03  // 右下角
#define MATRIX_START_CORNER       0x03  // 起始角落掩码

#define MATRIX_LAYOUT_HORIZONTAL  0x00  // 行优先布局
#define MATRIX_LAYOUT_VERTICAL    0x04  // 列优先布局
#define MATRIX_LAYOUT_AXIS        0x04  // 布局轴掩码

#define MATRIX_SCAN_PROGRESSIVE   0x00  // 连续扫描（每行的像素顺序相同）
#define MATRIX_SCAN_ZIGZAG        0x08  // 之字形扫描 （每行的像素顺序在行之间反转）
#define MATRIX_SCAN_SEQUENCE      0x08  // 扫描顺序掩码



//WS2812Matrix结构体子类
typedef struct {
    uint8_t matrixWidth;
    uint8_t matrixHeight;
    uint8_t pin;
    uint8_t matrixType;
} ws2812_matrix_config;

//ws2812_matrix_t 结构体表示 WS2812 灯条矩阵
typedef struct {
    ws2812_strip_t *strip;
    ws2812_matrix_config config;
} ws2812_matrix_t;


#endif


//ws2812 灯带模式函数声明
#ifdef CONFIG_WS2812_MODE_STRIP

/**
 * @brief  创建WS2812控制句柄
 * 
 * @return ws2812_strip_t* - WS2812控制句柄
 */
ws2812_strip_t* ws2812_create();

/**
 * @brief 设置单个LED的颜色
 * 
 * @param strip - WS2812控制句柄
 * @param index - LED的索引
 * @param color - LED的颜色
 */
void led_set_pixel(ws2812_strip_t *strip, int index, led_color_t color);

/**
 * @brief 使所有LED常亮
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 */
void led_set_on(ws2812_strip_t *strip, led_color_t color);

/**
 * @brief 关闭所有LED
 * 
 * @param strip - WS2812控制句柄
 */
void led_set_off(ws2812_strip_t *strip);

/**
 * @brief 使所有LED呼吸效果
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 */
void led_set_breath(ws2812_strip_t *strip, led_color_t color);

/**
 * @brief 使所有LED缓缓亮起
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 */
void led_set_fade_in(ws2812_strip_t *strip, led_color_t color);

/**
 * @brief 使所有LED微微闪烁
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 * @param count - 闪烁次数
 */
void led_set_blink_slow(ws2812_strip_t *strip, led_color_t color, int count);

/**
 * @brief 使所有LED快速闪烁
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 * @param count - 闪烁次数
 */
void led_set_blink_fast(ws2812_strip_t *strip, led_color_t color, int count);

/**
 * @brief 使所有LED彩虹效果
 * 
 * @param strip - WS2812控制句柄
 */
void led_set_rainbow(ws2812_strip_t *strip);

/**
 * @brief 自选LED效果
 * 
 * @param strip - WS2812控制句柄
 * @param color - LED的颜色
 * @param effect - LED的效果类型
 */
void ws2812_set(ws2812_strip_t *strip, led_color_t color, led_effect_t effect);

/**
 * @brief LED跑马灯效果
 * 
 * @param strip - WS2812控制句柄
 * @param index_start - 开始的LED索引
 * @param index_end   - 结束的LED索引
 * @param color       - LED的颜色
 * @param delay_ms    - 延迟时间
 */
void set_led_color_gradient(ws2812_strip_t *strip, int index_start, int index_end, led_color_t color, int delay_ms);

/**
 * @brief 更新LED显示
 * 
 * @param strip - WS2812控制句柄
 */
void update_led_display(ws2812_strip_t *strip);


//ws2812 矩阵屏幕模式函数声明
#elif defined(CONFIG_WS2812_MODE_MATRIX)
/**
 * @brief 创建WS2812矩阵控制句柄
 * 
 * @return ws2812_matrix_t*
 */
ws2812_matrix_t* ws2812_matrix_create();

/**
 * @brief 设置单个LED的颜色
 * 
 * @param matrix - WS2812控制句柄
 * @param x - led灯珠x坐标
 * @param y - led灯珠y坐标
 * @param color - LED的颜色
 */
void led_matrix_set_pixel(ws2812_matrix_t *matrix, int x, int y, led_color_t color);

/**
 * @brief 更新LED显示
 * 
 * @param matrix 
 */
void led_matrix_show(ws2812_matrix_t *matrix);

/**
 * @brief 清空LED显示
 * 
 * @param matrix 
 */
void led_matrix_clear(ws2812_matrix_t *matrix);

#endif

#ifdef __cplusplus
}
#endif

#endif // WS2812_CONTROL_H