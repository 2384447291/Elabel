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

// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

extern "C" void app_main(void)
{
    //初始化nvs
    nvs_init();
    //删除nvs信息
    erase_nvs();
    //获取nvs信息
    get_nvs_info();

    //电池管理器初始化，并拉起硬件开机
    BatteryManager::Instance()->init();

    //初始化wifi
    m_wifi_init();
    //连接wifi
    m_wifi_connect();
    //创建httpclient更新线程
    http_client_init();

    //初始化espnow
    EspNowClient::Instance()->init();

    //按键初始化
    ControlDriver::Instance()->init();
    //播放开机音乐
    // ControlDriver::Instance()->getBuzzer().playUponMusic();

    //创造gui线程，这里需要绑定到一个核上防止内存被破坏，这里优先绑定到核1上，如果蓝牙和wifi不用的情况下可以绑定到核0上
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
    //这里在调用完初始化后会紧跟一个刷新halfmind的函数(写在ui_init()函数里面)，默认为第一个界面

    //等待2slvgl初始化完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    //重新刷所有的语言
    lock_lvgl();
    Change_All_language();
    release_lvgl();

    ElabelController::Instance()->Init();//Elabel控制器初始化
    elabelUpdateTick = 0;

    while(1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        elabelUpdateTick += 10;
        //20ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
        //初始化的第一个状态机为init_state
        if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();
    }
}




