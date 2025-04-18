#include "ElabelController.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "lvgl.h"
#include "network.h"
#include "http.h"
#include "ota.h"
#include "m_mqtt.h"
#include "global_message.h"
#include "global_time.h"
#include "global_draw.h"
#include "global_nvs.h"
#include "esp_http_client.h"
#include "battery_manager.hpp"
#include "control_driver.hpp"
#include "esp_now_client.hpp"
#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"
#include "codec.hpp"
#include "esp_sleep.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_wifi.h"
// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 
// extern "C" void app_main(void)
// {
//     // esp_deep_sleep_start(); 
//     // // 安装GPIO中断服务
//     // gpio_install_isr_service(0);
//     // //初始化nvs
//     // nvs_init();
//     // //删除nvs信息
//     // // erase_nvs();
//     // //获取nvs信息
//     // get_nvs_info();

//     // //初始化codec
//     // // MCodec::Instance()->init();
//     // //播放音乐
//     // // MCodec::Instance()->play_music("open");

//     // // // //电池管理器初始化，并拉起硬件开机
//     // BatteryManager::Instance()->init(); 
//     // vTaskDelay(2000 / portTICK_PERIOD_MS);

//     // //初始化wifi
//     // m_wifi_init();
//     // //如果是从机就不需要连接wifi和创建http线程
//     // if(get_global_data()->m_is_host != 2)
//     // {
//     //     m_wifi_connect();
//     //     //创建httpclient更新线程
//     //     http_client_init();
//     // }
    
//     // //按键初始化
//     // // ControlDriver::Instance()->init();

//     // //创造gui线程，这里需要绑定到一个核上防止内存被破坏，这里优先绑定到核1上，如果蓝牙和wifi不用的情况下可以绑定到核0上
//     // // xTaskCreatePinnedToCore(guiTask, "gui", 8192, NULL, 0, NULL, 1);
//     // //这里在调用完初始化后会紧跟一个刷新halfmind的函数(写在ui_init()函数里面)，默认为第一个界面
//     // //等待2slvgl初始化完成
//     // vTaskDelay(2000 / portTICK_PERIOD_MS);

//     // //初始化espnow
//     // // EspNowClient::Instance()->init();

//     // vTaskDelay(2000 / portTICK_PERIOD_MS);

//     // ElabelController::Instance()->Init();//Elabel控制器初始化
//     // elabelUpdateTick = 0;
//     while(1)
//     {
//         // elabelUpdateTick += 10;
//         // //5ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
//         // //初始化的第一个状态机为init_state
//         // if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();

//         // 打开监听时间窗
//         vTaskDelay(pdMS_TO_TICKS(1000));

//         // 配置唤醒定时器
//         rtc_gpio_isolate(GPIO_NUM_0);
//         rtc_gpio_isolate(GPIO_NUM_1);
//         rtc_gpio_isolate(GPIO_NUM_2);
//         rtc_gpio_isolate(GPIO_NUM_3);
//         rtc_gpio_isolate(GPIO_NUM_4);
//         rtc_gpio_isolate(GPIO_NUM_5);
//         rtc_gpio_isolate(GPIO_NUM_6);
//         rtc_gpio_isolate(GPIO_NUM_7);
//         rtc_gpio_isolate(GPIO_NUM_8);
//         rtc_gpio_isolate(GPIO_NUM_9);
//         rtc_gpio_isolate(GPIO_NUM_10);
//         rtc_gpio_isolate(GPIO_NUM_11);
//         rtc_gpio_isolate(GPIO_NUM_12);
//         rtc_gpio_isolate(GPIO_NUM_13);
//         rtc_gpio_isolate(GPIO_NUM_14);
//         rtc_gpio_isolate(GPIO_NUM_16);
//         rtc_gpio_isolate(GPIO_NUM_17);
//         rtc_gpio_isolate(GPIO_NUM_18);
//         rtc_gpio_isolate(GPIO_NUM_19);
//         rtc_gpio_isolate(GPIO_NUM_20);
//         rtc_gpio_isolate(GPIO_NUM_21);

//         // 配置电池开关控制引脚（输出）
//         gpio_config_t io_conf = {};
//         io_conf.pin_bit_mask = (1ULL << BATTERY_POWER_CTRL);
//         io_conf.mode = GPIO_MODE_OUTPUT;
//         io_conf.intr_type = GPIO_INTR_DISABLE;
//         io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
//         io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
//         gpio_config(&io_conf);
//         gpio_set_level(BATTERY_POWER_CTRL, 1);

//         ESP_ERROR_CHECK(gpio_hold_en(BATTERY_POWER_CTRL));

//         gpio_deep_sleep_hold_en();

//         esp_sleep_enable_timer_wakeup(10 * 1000000ULL);  // 5 秒
//         esp_deep_sleep_start();  // 会自动恢复后继续while循环
//     }
// }


extern "C" void app_main(void)
{
    nvs_flash_init();
    esp_event_loop_create_default();  // 创建默认事件循环，用于接收处理wifi相关事件
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 使用默认参数配置wifi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // 将配置丢进去，初始化wifi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置wifi模式为station模式
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));// 设置wifi存储位置（存在ram里意味着。断电不保存）
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));// 设置当前 WiFi 省电类型（这里为不省电）
    ESP_ERROR_CHECK(esp_wifi_start());// 开启wifi
    while(1)
    {
        ESP_ERROR_CHECK(esp_netif_init()); // 初始化底层 TCP/IP 协议栈。
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_sleep_enable_timer_wakeup(30000000); // 30秒后唤醒
        esp_light_sleep_start();
    }
}
