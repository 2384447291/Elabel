// #include "ElabelController.hpp"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/timers.h"

// #include "lvgl.h"
// #include "network.h"
// #include "http.h"
// #include "ota.h"
// #include "m_mqtt.h"
// #include "global_message.h"
// #include "global_time.h"
// #include "global_draw.h"
// #include "global_nvs.h"
// #include "esp_http_client.h"
// #include "battery_manager.hpp"
// #include "control_driver.hpp"
// #include "esp_now_client.hpp"
// #include "esp_now_host.hpp"
// #include "esp_now_slave.hpp"
// #include "codec.hpp"
// #include "esp_sleep.h"
// #include "esp_bt.h"
// #include "esp_bt_main.h"
// #include "esp_bt_device.h"
// #include "esp_wifi.h"

// #include "esp_sleep.h"
// #include "esp_pm.h"
// #include "driver/rtc_io.h"
// #include "esp_private/sleep_cpu.h"
// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

// static void light_sleep_task(void *args);

// extern "C" void app_main(void)
// {
//     // //安装GPIO中断服务
//     // gpio_install_isr_service(0);
//     // //初始化nvs
//     // nvs_init();
//     // //删除nvs信息
//     // // erase_nvs();
//     // //获取nvs信息
//     // get_nvs_info();
    
//     // //初始化espnow
//     // // EspNowClient::Instance()->init();

//     // m_wifi_init();

//     // gpio_config_t io_conf = {};
//     // io_conf.pin_bit_mask = (1ULL << GPIO_NUM_20);
//     // io_conf.mode = GPIO_MODE_OUTPUT;
//     // io_conf.intr_type = GPIO_INTR_DISABLE;
//     // io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
//     // io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
//     // gpio_config(&io_conf);
//     // gpio_set_level(GPIO_NUM_20, 0);

//     // // //太神奇了把这个打开之后功耗从340ua降到230ua
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   
//     // //太叼了不on，off不了，这就是esp32

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL32K, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL32K, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_OFF);

//     // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
//     // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

//     esp_pm_config_t pm_config = {
//             .max_freq_mhz = 80,
//             .min_freq_mhz = 10,
//             //这个表示是否要自动进入light sleep，我们都是手动的所以不用开启
//             .light_sleep_enable = true,
//     };
//     ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

//     vTaskDelay(4000 / portTICK_PERIOD_MS);

//     xTaskCreate(light_sleep_task, "light_sleep_task", 4096, NULL, 6, NULL);
// }

// static void light_sleep_task(void *args)
// {
//     while (true) 
//     {
//         printf("Entering light sleep\n");
//         // 配置定时唤醒时间（微秒）
//         ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(20000000));
//         esp_wifi_start();
//         ESP_ERROR_CHECK(esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE));
//         vTaskDelay(20 / portTICK_PERIOD_MS);
//         esp_wifi_stop();
//         esp_light_sleep_start();
//     }
//     vTaskDelete(NULL);
// }


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char *TAG = "main";

// 电源管理锁（防止light sleep）
static esp_pm_lock_handle_t s_pm_lock;

static void pm_config_init(void)
{
    esp_pm_config_esp32c6_t pm_cfg = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg));

    // 创建锁，名称可自定义（最多16字节），锁住频率不降和 light sleep
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "no_sleep", &s_pm_lock));
}

extern "C" void app_main(void)
{
    pm_config_init();

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_20);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    while (true) {
        // 模拟唤醒后的处理
        ESP_LOGI(TAG, "Waking up: lock sleep and turn on GPIO");

        // 锁住 light sleep
        ESP_ERROR_CHECK(esp_pm_lock_acquire(s_pm_lock));

        gpio_set_level(GPIO_NUM_20, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));  // 保持1秒亮灯

        gpio_set_level(GPIO_NUM_20, 0);
        ESP_LOGI(TAG, "Turned off GPIO, unlock sleep");

        // 解锁，允许进入 light sleep
        ESP_ERROR_CHECK(esp_pm_lock_release(s_pm_lock));

        // 休眠模拟
        vTaskDelay(pdMS_TO_TICKS(2000));  // 系统可以进入 light sleep
    }
}
