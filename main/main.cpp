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

#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#undef ESP_LOGI
#define ESP_LOGI(tag, format, ...) 
extern "C" void app_main(void)
{
    // 安装GPIO中断服务
    gpio_install_isr_service(0);
    //初始化nvs
    nvs_init();
    //删除nvs信息
    // erase_nvs();
    //获取nvs信息
    get_nvs_info();

    //初始化wifi
    m_wifi_init();
    
    //初始化espnow
    EspNowClient::Instance()->init();

    esp_bluedroid_disable();
    esp_bt_controller_disable();
    esp_wifi_stop();
    // //太神奇了把这个打开之后功耗从340ua降到262ua
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   
    //太叼了不on，off不了，这就是esp32
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL32K, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL32K, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_OFF);

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    

    esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,
            .min_freq_mhz = 10,
            //这个表示是否要自动进入light sleep，我们都是手动的所以不用开启
            .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    while(1)
    {
        // 配置定时唤醒时间（微秒）
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(5000000));
        esp_wifi_start();
        uint8_t data = 0;
        
        esp_wifi_stop();
        esp_light_sleep_start();
    }
}