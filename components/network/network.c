#include "network.h"
#include "blufi.h"

#define WIFI_CONNECT "WIFI_CONNECT"
#include "global_message.h"

bool is_wifi_init = false;
bool is_wifi_preprocess = false;

static void wifi_init_sta(void);
static void wifi_preprocess(void);

bool Is_connect_to_phone(void)
{
    return is_connect_to_phone;
}

void set_is_connect_to_phone(bool _is_connect_to_phone)
{
    is_connect_to_phone = _is_connect_to_phone;
}

uint8_t get_wifi_status(void)
{
    return wifi_state;
}

void set_wifi_status(uint8_t _wifi_state)
{
    wifi_state = _wifi_state;
}

void start_blue_activate()
{
    is_connect_to_phone = false;
    m_wifi_disconnect();
    if(blufi_notify_flag) return;
    //允许省电模式，否则不能wifi，蓝牙混用
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    start_blufi();
}
void stop_blue_activate()
{
    blufi_notify_flag = false;
}


void m_wifi_connect(void)
{   
    enable_reconnect = true;
    if(is_wifi_init == false || wifi_state != 0x00)
    {
        return;
    }
    //...................................判断是否能找到历史记录..........................................//
    if(strlen(get_global_data()->m_wifi_ssid) != 0 && strlen(get_global_data()->m_wifi_password) != 0)
    {
        wifi_config_t wifi_config = {
                        .sta = {
                            .ssid = "",
                            .password = "",
                        },
                    };
        bzero(&wifi_config, sizeof(wifi_config_t)); /* 将结构体数据清零 */
        memcpy(wifi_config.sta.ssid, get_global_data()->m_wifi_ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, get_global_data()->m_wifi_password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        wifi_state = 0x01;
        ESP_LOGI(WIFI_CONNECT,"Start connecting wifi with Password: %s and ssid %s \n", get_global_data()->m_wifi_password, get_global_data()->m_wifi_ssid);
    }
    else
    {
        ESP_LOGE(WIFI_CONNECT,"No WIFI History");
        wifi_state = 0x00;
    }
    //...................................判断是否能找到历史记录..........................................//
}

void m_wifi_disconnect(void)
{
    if(is_wifi_init == false || wifi_state == 0x00)
    {
        return;
    }
    enable_reconnect = false;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    wifi_state = 0x00;
    ESP_LOGI(WIFI_CONNECT,"Successful_disconnect_wifi.\n");
}

void m_wifi_init(void)
{
    if(!is_wifi_preprocess)
    {
        is_wifi_preprocess = true;
        wifi_preprocess();
    }
    if(!is_wifi_init)
    {
        wifi_init_sta();  
        is_wifi_init = true;
    }
    ESP_LOGI(WIFI_CONNECT,"Wifi_init_sta finished. \n");
}

void m_wifi_deinit(void)
{
    if(wifi_state!=0x00)
    {
        ESP_LOGE(WIFI_CONNECT,"Deinit false, wifi is not disconnect.\n");
        return;
    } 
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    is_wifi_init = false;
    ESP_LOGI(WIFI_CONNECT,"Successful_deinit_wifi.\n");
}

void start_blufi(void)
{
    ESP_LOGI(GATTS_TABLE_TAG,"bluficonfig start ....... \n");

    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_SSID], sizeof(wifi_ssid), (const uint8_t*)wifi_ssid);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_PASS], sizeof(wifi_passwd), (const uint8_t*)wifi_passwd);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_CUSTOM], sizeof(customer), (const uint8_t*)customer);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_STATE], sizeof(wifi_state), (const uint8_t*)&wifi_state);

    if(!is_blufi_init)
    {
        xTaskCreate(blufi_notify, "blufi_notify", 8192, NULL, 10, pTask_blufi);

        blufi_notify_flag = true;

        is_blufi_init = true;

        esp_err_t ret;

        if(!is_release_classic_ble)
        {
            is_release_classic_ble = true;
            // 释放经典蓝牙资源，保证设备不工作在经典蓝牙下面
            ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));   
        }
   
        // 初始化蓝牙控制器
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        } 

        // 使能蓝牙控制器，工作在 BLE mode
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        }  

        // 初始化蓝牙主机，使能蓝牙主机
        ret = esp_bluedroid_init();
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        ret = esp_bluedroid_enable();
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }

        // 注册 GATT 回调函数
        ret = esp_ble_gatts_register_callback(gatts_event_handler);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gatts register error, error code = %x", ret);
            return;
        }

        // 注册 GAP 回调函数, 蓝牙是通过GAP建立通信的(广播)
        ret = esp_ble_gap_register_callback(gap_event_handler);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gap register error, error code = %x", ret);
            return;
        }

        //注册 service 
        ret = esp_ble_gatts_app_register(ESP_APP_ID);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error, error code = %x", ret);
            return;
        }
        //设置 mtu
        esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
        if (local_mtu_ret){
            ESP_LOGE(GATTS_TABLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
        }
    }
}

/* 系统事件循环处理函数 */
static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    static int retry_num = 0;           /* 记录wifi重连次数 */
    /* 系统事件为WiFi事件 */
    if (event_base == WIFI_EVENT) 
    {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) /* 当wifi失去STA连接 */ 
        {
            if(!enable_reconnect) return;
            wifi_state = 0x01;
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(WIFI_CONNECT,"retry to connect to the AP %d times. \n",retry_num);
            ESP_LOGI(WIFI_CONNECT,"Retry connecting wifi with Password: %s and ssid %s\n", get_global_data()->m_wifi_password, get_global_data()->m_wifi_ssid);
            if (retry_num > 10)  /* WiFi重连次数大于10 */
            {
                /* 将WiFi连接事件标志组的WiFi连接失败事件位置1 */
                ESP_LOGE(WIFI_CONNECT,"Fail connecting wifi with Password: %s and ssid %s\n", get_global_data()->m_wifi_password, get_global_data()->m_wifi_ssid);
                // 清零 wifi_state
                wifi_state = 0x00;
                retry_num = 0;
                ESP_ERROR_CHECK(esp_wifi_disconnect());
            }
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED) /* 当wifi成功连接 */ 
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; /* 获取IP地址信息*/
            ESP_LOGI(WIFI_CONNECT,"got ip:%d.%d.%d.%d \n" , IP2STR(&event->ip_info.ip));  /* 打印ip地址*/
            retry_num = 0;                                              /* WiFi重连次数清零 */
            set_nvs_info("wifi_ssid",get_global_data()->m_wifi_ssid);
            set_nvs_info("wifi_passwd",get_global_data()->m_wifi_password);
            wifi_state = 0x02;
            uint8_t wifi_channel = 0;
            wifi_second_chan_t wifi_second_channel = WIFI_SECOND_CHAN_NONE;
            ESP_ERROR_CHECK(esp_wifi_get_channel(&wifi_channel, &wifi_second_channel));
            ESP_LOGI(WIFI_CONNECT,"wifi channel: %d.", wifi_channel);
        }
    }
}

static void wifi_preprocess(void)
{
    //...................................初始化wifi连接handler..........................................//
    /* 初始化底层TCP/IP堆栈。在应用程序启动时，应该调用此函数一次。*/
    ESP_ERROR_CHECK(esp_netif_init());
    /* 创建一个事件循环 */  
    // 调用 esp_event_handler_register 前，必须先调用 esp_event_loop_create_default()
    // 因为事件处理程序(esp_event_handler_register)需要一个已初始化的事件循环来接收和处理事件。
    // 如果没有创建事件循环，事件处理程序无法正常工作。
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	/* 将事件处理程序注册到系统默认事件循环，分别是WiFi事件 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // 设置通用sta处理函数
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
    /* 创建STA */
    esp_netif_create_default_wifi_sta();
    //...................................初始化wifi连接handler..........................................//
}


static void wifi_init_sta(void)
{
    wifi_state = 0x00;
    is_blufi_init = false;
    //......................................初始化wifi..........................................//
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* 根据cfg参数初始化wifi连接所需要的资源 */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* 设置WiFi的工作模式为 AP+STA, 用来兼容espnow  如果单用STA模式，wifi在不工作的时候station模式
    会放弃监听，也就检测不到espnow消息则无法使用espnow */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    /* 启动WiFi连接 */
    ESP_ERROR_CHECK(esp_wifi_start());
    /* 设置wifi存储模式为RAM，不保存wifi信息到nvs，默认是保存到nvs */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    /* 防止在省电模式，wifi质量不好 */ 
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    //......................................初始化wifi..........................................//
}
