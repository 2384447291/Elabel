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
#include "http.h"
#include "network.h"

#include "ElabelController.hpp"

void reset_elabel()
{
    erase_nvs();
    esp_restart();
}

extern "C" void app_main(void)
{
    //初始化电池管理
    BatteryManager::Instance()->init();

    //等待电路初始化
    vTaskDelay(pdMS_TO_TICKS(500));

    //初始化gui
    Gui_init();
    //等待lvgl初始化
    vTaskDelay(pdMS_TO_TICKS(2000));

    //安装GPIO中断服务
    gpio_install_isr_service(0);
    //初始化nvs
    nvs_init();
    //删除nvs信息
    // erase_nvs();
    //获取nvs信息
    get_nvs_info();

    //初始化按键
    ControlDriver::Instance()->init();

    //初始化wifi
    m_wifi_init();
    if(get_global_data()->m_is_host)
    {
        //初始化http客户端
        http_client_init();
    }

    //初始化Elabel控制器
    ElabelController::Instance()->Init();//Elabel控制器初始化
    elabelUpdateTick = 0;

    //初始化codec
    MCodec::Instance()->init();
    //播放音乐
    MCodec::Instance()->play_music("open");

    //注册按键回调
    ControlDriver::Instance()->button_press_together_15.Togetherlongpress.registerCallback(reset_elabel);

    while (true) 
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        elabelUpdateTick += 10;
        //5ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
        //初始化的第一个状态机为init_state
        if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();
    }
}