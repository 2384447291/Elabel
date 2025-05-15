// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_pm.h"
// #include "esp_log.h"
// #include "esp_timer.h"
// #include "driver/gpio.h"

// #include "global_message.h"
// #include "global_time.h"
// #include "global_draw.h"
// #include "global_nvs.h"

// #include "esp_now_client.hpp"
// #include "control_driver.hpp"
// #include "battery_manager.hpp"
// #include "http.h"
// #include "network.h"

// #include "ElabelController.hpp"

// void play_music()
// {
//     MCodec::Instance()->play_music("open");
// }

// extern "C" void app_main(void)
// {
//     //初始化电池管理
//     BatteryManager::Instance()->init();

//     //等待电路初始化
//     vTaskDelay(pdMS_TO_TICKS(2000));

//     //安装GPIO中断服务
//     gpio_install_isr_service(0);
//     //初始化nvs
//     nvs_init();
//     //删除nvs信息
    
//     // erase_nvs();
//     //获取nvs信息
//     get_nvs_info();

//     //初始化按键
//     ControlDriver::Instance()->init();

//     //初始化gui
//     Gui_init();
//     //等待lvgl初始化
//     vTaskDelay(pdMS_TO_TICKS(2000));

//     //初始化wifi
//     m_wifi_init();
//     //连接wifi
//     m_wifi_connect();
//     //创建httpclient更新线程
//     http_client_init();

//     ElabelController::Instance()->Init();//Elabel控制器初始化
//     elabelUpdateTick = 0;

//     ControlDriver::Instance()->button1.CallbackLongPress.registerCallback(play_music);

//     //初始化codec
//     MCodec::Instance()->init();
//     //播放音乐
//     MCodec::Instance()->play_music("open");

//     while (true) 
//     {
//         vTaskDelay(20 / portTICK_PERIOD_MS);
//         elabelUpdateTick += 20;
//         //5ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
//         //初始化的第一个状态机为init_state
//         if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();
//     }
// }

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "global_message.h"
#include "global_time.h"
#include "global_draw.h"
#include "global_nvs.h"

#include "esp_now_client.hpp"
#include "control_driver.hpp"
#include "battery_manager.hpp"

extern "C" void app_main(void)
{
    //等待电路初始化
    vTaskDelay(pdMS_TO_TICKS(2000));
    //安装GPIO中断服务
    gpio_install_isr_service(0);
    //初始化nvs
    nvs_init();
    //删除nvs信息
    // erase_nvs();
    //获取nvs信息
    get_nvs_info();
    // //初始化电池管理
    BatteryManager::Instance()->init();
    // //初始化按键
    // // ControlDriver::Instance()->init();
    //初始化gui
    Gui_init();
    // //初始化音频
    MCodec::Instance()->init();
    MCodec::Instance()->play_music("ding");
    // //初始化网络
    // // m_wifi_init();
    // //初始化espnow
    // // EspNowClient::Instance()->init();
    // // 等待所有组件初始化完成

    vTaskDelay(pdMS_TO_TICKS(8000));
    suspend_gui();
    BatteryManager::Instance()->setPowerState(false);
    gpio_hold_en(DEV_POWER_CTRL);
    // MCodec::Instance()->close_dev();
    bool is_shutdown = false;
    //用来debug的接口
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        ESP_ERROR_CHECK(esp_pm_lock_acquire(BatteryManager::Instance()->s_pm_lock));
        resume_gui(); 
        gpio_hold_dis(DEV_POWER_CTRL);
        BatteryManager::Instance()->setPowerState(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(!is_shutdown)
        {
            switch_screen(ui_ShutdownScreen);
            is_shutdown = true;
        }
        else
        {
            switch_screen(ui_HalfmindScreen);
            is_shutdown = false;
        }
        MCodec::Instance()->play_music("ding");
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_ERROR_CHECK(esp_pm_lock_release(BatteryManager::Instance()->s_pm_lock));

        suspend_gui();
        BatteryManager::Instance()->setPowerState(false);
        gpio_hold_en(DEV_POWER_CTRL);        
    }
}

