#include "ElabelController.hpp"
#include "m_esp_now.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "ui.h"
#include "lvgl_helpers.h"
#include "ssd1680.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "network.h"
#include "http.h"
#include "ota.h"
#include "m_mqtt.h"
#include "global_message.h"
#include "esp_http_client.h"
#include "control_driver.h"


static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

bool is_elabel_init = false;

TimerHandle_t updateTimer;

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
        ElabelStateSet(ELSE_STATE);
        _ui_screen_change(&ui_OTAScreen,LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_OTAScreen_screen_init);
        lv_label_set_text(ui_Label3, get_global_data()->version);

        uint32_t ota_Wait_tick = 0;
        while(true)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ota_Wait_tick++;
            EventBits_t bits;
            bits = xEventGroupWaitBits(get_eventgroupe(), BUTTON_TASK_BIT | ENCODER_TASK_BIT, pdTRUE, pdFALSE, 0); //不阻塞等待
            if (bits & BUTTON_TASK_BIT) {
                button_interrupt temp_interrupt = get_button_interrupt();
                if(temp_interrupt == short_press)
                {
                    start_ota();
                    break;
                }
            }
            if(bits & ENCODER_TASK_BIT)
            {
                encoder_interrupt temp_interrupt = get_encoder_interrupt();
                break;
            }
            if(ota_Wait_tick >= 300)
            {
                break;
            }
        }
    }
    else{
        ESP_LOGI("OTA", "No need OTA, newest version");
    }

    http_get_todo_list(true);
    http_bind_user(true);

    // m_espnow_init();  

    ESP_LOGE("fuck fuck","start your new day!!!\n");
}

void print_memory_status() {
    size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free_mem = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    size_t internal_free_mem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI("MEMORY", "Free memory: %u bytes", free_mem);
    ESP_LOGI("MEMORY", "Minimum free memory: %u bytes", min_free_mem);
    ESP_LOGI("MEMORY", "Internal free memory: %u bytes", internal_free_mem);
}


/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    //lvgl初始化
    lv_init();

    //lvgl硬件初始化定义在lvgl_esp32_drivers，这个函数里面会根据宏定义调用自己定义的ssd1680的函数
    lvgl_driver_init();

    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),MALLOC_CAP_DMA); //MALLOC_CAP_DMA);MALLOC_CAP_SPIRAM

    assert(buf1 != NULL);

    static lv_color_t *buf2 = NULL;

    static lv_disp_draw_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = 250;
    disp_drv.ver_res = 122;


    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
    disp_drv.set_px_cb = disp_driver_set_px;
    disp_drv.rounder_cb = disp_driver_rounder;

    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);


    //--------------4.创建并启动一个定时器，用于定期调用 lv_tick_task 函数(每隔 1000 毫秒触发一次),来告诉lvgl过了1s--------------//
    esp_timer_handle_t periodic_timer;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = [](void *arg){
        (void)arg;
        lv_tick_inc(1);},
        .name = "periodic_gui"};
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));
    //--------------4.创建并启动一个定时器，用于定期调用 lv_tick_task 函数(每隔 1000 毫秒触发一次),来告诉lvgl过了1s--------------//

    /* Create the demo application */
    create_demo_application();
    lv_refr_now(NULL);
    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            // printf("before lv_task_handler\n");
            lv_task_handler();
            // printf("lv_task_handler\n");
            xSemaphoreGive(xGuiSemaphore);
       }
    }
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}

static void create_demo_application(void)
{
    printf("Creating the demo application\n");
    ui_init();
}

extern "C" void app_main(void)
{
    m_wifi_init();//初始化wifi
    m_wifi_connect();//连接wifi
    http_client_init();//并创建httpclient更新线程

    // control_gpio_init();//按键初始化
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("Starting LVGL example\n");
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
    // print_memory_status();
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    ElabelController::Instance()->Init();//Elabel控制器初始化
    // print_memory_status();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ElabelStateSet(HALFMIND_STATE);
    // _ui_screen_change(&ui_FocusScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_FocusScreen_screen_init);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    _ui_screen_change(&ui_HalfmindScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_HalfmindScreen_screen_init);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // print_memory_status();
    elabelUpdateTick = 0;
    while(1)
    {
        if(elabelUpdateTick % 100 == 0)
        {
            printf("elabelUpdateTick = %d",elabelUpdateTick);
        }
        // 检查 Wi-Fi 状态
        if (get_wifi_status() == 2) {
            e_label_init();

        }
        else if(get_wifi_status() == 0)
        {
            ElabelStateSet(QRCODE_STATE);
            _ui_screen_change(&ui_FocusScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_FocusScreen_screen_init);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            _ui_screen_change(&ui_HalfmindScreen, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_HalfmindScreen_screen_init);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            ESP_LOGI("fuvk`","wifi is not connected\n");
            continue;
        }
        else
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            elabelUpdateTick++;
            continue;
        }

        ElabelController::Instance()->Update();


        vTaskDelay(10 / portTICK_PERIOD_MS);
        elabelUpdateTick++;
    }
}


