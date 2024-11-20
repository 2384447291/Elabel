#include "network.h"
#include "http.h"
#include "ota.h"
#include "m_mqtt.h"
#include "global_message.h"
#include "esp_http_client.h"
#include "control_driver.h"
#include "ElabelController.hpp"
#include "m_esp_now.h"
#include "lvgl.h"

bool is_elabel_init = false;

void e_label_init()
{
    if(is_elabel_init) return;
    is_elabel_init = true;
    //等待连接两秒稳定
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    //同步时间(堵塞等待)
    HTTP_syset_time();
    get_unix_time();

    //mqtt服务器初始化
    mqtt_client_init();

    //获取最新版本固件
    http_get_latest_version(true);
    if(get_global_data()->newest_firmware_url!=NULL)
    {
        start_ota();
    }
    else{
        ESP_LOGI("OTA", "No need OTA, newest version");
    }

    http_get_todo_list(true);
    http_bind_user(true);

    // m_espnow_init();  

    ESP_LOGE("fuck fuck","start your new day!!!\n");
}

extern "C" void app_main(void)
{
    m_wifi_init();//初始化wifi
    m_wifi_connect();//连接wifi
    http_client_init();//并创建httpclient更新线程

    // control_gpio_init();//按键初始化

    ElabelController::Instance()->Init();//Elabel控制器初始化

    while(1)
    {
        vTaskDelay(10/ portTICK_PERIOD_MS);
        ElabelController::Instance()->Update();
        //当连上网之后有一些重要事情要做，比如获取最新版本
        if(get_wifi_status() == 2)
        {
            e_label_init();
        }
    }
}

