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
#include "esp_now_host.hpp"
#include "esp_now_slave.hpp"

extern "C" void app_main(void)
{
    //安装GPIO中断服务
    gpio_install_isr_service(0);
    //初始化nvs
    nvs_init();
    //获取nvs信息
    get_nvs_info();

    //初始化按键
    ControlDriver::Instance()->init();

    //初始化wifi
    m_wifi_init();

    //初始化espnow
    EspNowClient::Instance()->init();

    //因为和后面的初始化强相关所以提前初始化
    if(get_global_data()->m_is_host == 2)
    {
        //初始化EspNowSlave
        EspNowSlave::Instance()->init(get_global_data()->m_host_mac, get_global_data()->m_host_channel, get_global_data()->m_userName);
    }
    else 
    {
        //初始化http客户端
        http_client_init();
    }

    //等待wifi初始化完毕
    vTaskDelay(pdMS_TO_TICKS(1000));

    //初始化Elabel控制器
    ElabelController::Instance()->Init();//Elabel控制器初始化
    elabelUpdateTick = 0;

    //初始化电池管理
    BatteryManager::Instance()->init();

    //等待电路初始化
    vTaskDelay(pdMS_TO_TICKS(500));

    //如果电池电压低于3.3V且没有连接线材，则重启次数+1，清零在后面
    if(BatteryManager::Instance()->getBatteryLevel() < 3.3f && !BatteryManager::Instance()->is_usb_connected())
    {
        get_global_data()->reset_count++;
        set_reset_count(get_global_data()->reset_count);
    }

    //初始化gui
    Gui_init();
    //等待lvgl初始化
    vTaskDelay(pdMS_TO_TICKS(2000));

    //如果5次没有启动成功则不运行
    if(get_global_data()->reset_count >= 5)
    {
        lock_lvgl();
        switch_screen(ui_ShutdownScreen);
        release_lvgl();
        while(1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    //初始化codec
    MCodec::Instance()->init();

    //注册按键回调
    ControlDriver::Instance()->button_press_together_48.Togetherlongpress.registerCallback(force_reset_elabel);

    ControlDriver::Instance()->register_all_button_callback(play_button_sound);

    while (true) 
    {
        vTaskDelay(20 / portTICK_PERIOD_MS);
        elabelUpdateTick += 20;
        //5ms更新一次,这个函数在初始化后会阻塞,出初始化后elabelUpdateTick会再次置零
        //初始化的第一个状态机为init_state
        if(elabelUpdateTick%20==0) ElabelController::Instance()->Update();
    }
}