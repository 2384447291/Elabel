#include "global_nvs.h"
#include "global_message.h"
#include "esp_log.h"
#define NVS_TAG "NVS_TAG"
#define NVS_HANDLER "Elabel_cfg"

bool is_nvs_init = false;

void get_nvs_info(void)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }

    nvs_handle wificfg_nvs_handler; /* 定义一个NVS操作句柄 */
    ESP_ERROR_CHECK(nvs_open(NVS_HANDLER, NVS_READWRITE, &wificfg_nvs_handler) );//打开一个名叫"Elabel_cfg"的可读可写nvs空间

    //--------------------------从nvs中获取wifi_ssid--------------------------------//
    size_t len;                   
    len = sizeof(get_global_data()->m_wifi_ssid);  
    esp_err_t ssid_err = nvs_get_str(wificfg_nvs_handler,"wifi_ssid",get_global_data()->m_wifi_ssid,&len) ;
    if(ssid_err != ESP_OK) ESP_LOGE(NVS_TAG,"No history wifi_ssid found. \n");
    else ESP_LOGI(NVS_TAG,"history wifi_ssid found : %s. \n", get_global_data()->m_wifi_ssid);

    //--------------------------从nvs中获取wifi_password--------------------------------//
    len = sizeof(get_global_data()->m_wifi_password);      
    esp_err_t passwd_err = nvs_get_str(wificfg_nvs_handler,"wifi_passwd",get_global_data()->m_wifi_password,&len) ;
    if(passwd_err != ESP_OK) ESP_LOGE(NVS_TAG,"No history wifi_passwd found. \n");
    else ESP_LOGI(NVS_TAG,"history wifi_passwd found : %s. \n", get_global_data()->m_wifi_password);

    //--------------------------从nvs中获取customer--------------------------------//
    len = sizeof(get_global_data()->m_usertoken);      
    esp_err_t customer_err = nvs_get_str(wificfg_nvs_handler,"customer",get_global_data()->m_usertoken,&len) ;
    if(customer_err != ESP_OK) ESP_LOGE(NVS_TAG,"No customer found. \n");
    else ESP_LOGI(NVS_TAG,"history customer found : %s. \n", get_global_data()->m_usertoken);

    //--------------------------从nvs中获取username--------------------------------//
    len = sizeof(get_global_data()->m_userName);     
    esp_err_t username_err = nvs_get_str(wificfg_nvs_handler,"username",get_global_data()->m_userName,&len) ;
    if(username_err != ESP_OK) ESP_LOGE(NVS_TAG,"No username found. \n");
    else ESP_LOGI(NVS_TAG,"history username found : %s. \n", get_global_data()->m_userName);

    //--------------------------从nvs中获取language--------------------------------//   
    len = sizeof(get_global_data()->m_language);      
    char language_str[len];
    esp_err_t language_err = nvs_get_str(wificfg_nvs_handler,"language",language_str,&len) ;
    if(language_err != ESP_OK) ESP_LOGE(NVS_TAG,"No language found. \n");
    else 
    {
        ESP_LOGI(NVS_TAG,"history language found : %s. \n", language_str);
        //如果language_str为"0"，则设置为English，否则设置为Chinese
        if(strcmp(language_str,"0") == 0)
        {
        get_global_data()->m_language = English;
        }
        else if(strcmp(language_str,"1") == 0)
        {
            get_global_data()->m_language = Chinese;
        }
        else
        {
            get_global_data()->m_language = English;
        }
    }

    //--------------------------从nvs中获取is_host--------------------------------//
    len = sizeof(get_global_data()->m_is_host);     
    char is_host_str[len];
    esp_err_t is_host_err = nvs_get_str(wificfg_nvs_handler,"is_host",is_host_str,&len) ;
    if(is_host_err != ESP_OK) ESP_LOGE(NVS_TAG,"No is_host found. \n");
    else 
    {
        ESP_LOGI(NVS_TAG,"history is_host found : %s. \n", is_host_str);
        //如果is_host_str为"0"，则设置为0，否则设置为1
        if(strcmp(is_host_str,"1") == 1)
        {
        get_global_data()->m_is_host = 1;
        }
        else if(strcmp(is_host_str,"2") == 2)
        {
            get_global_data()->m_is_host = 2;
        }
        else
        {
            get_global_data()->m_is_host = 0;
        }
    }

    //--------------------------从nvs中获取host_mac--------------------------------//
    if(get_global_data()->m_is_host == 1)
    {
        //如果判断为主机，则加载从机地址
        len = sizeof(get_global_data()->m_slave_mac) + sizeof(get_global_data()->m_slave_num);     
        char slave_mac_str[len];
        esp_err_t slave_mac_err = nvs_get_str(wificfg_nvs_handler,"slave_mac",slave_mac_str,&len) ;
        if(slave_mac_err != ESP_OK) ESP_LOGE(NVS_TAG,"Device is host, But no slave_mac found. \n");
        else 
        {
            ESP_LOGI(NVS_TAG,"Device is host, history found slave_num is %d. \n", slave_mac_str[0]);
            get_global_data()->m_slave_num = slave_mac_str[0];
            for(int i = 0; i < get_global_data()->m_slave_num; i++)
            {
                memcpy(get_global_data()->m_slave_mac[i], &slave_mac_str[i*6+1], 6);
            }
        }

    }
    else if(get_global_data()->m_is_host == 2)
    {
        //如果判断为从机，则加载主机地址
        len = sizeof(get_global_data()->m_host_mac);     
        char host_mac_str[len];
        esp_err_t host_mac_err = nvs_get_str(wificfg_nvs_handler,"host_mac",host_mac_str,&len) ;
        if(host_mac_err != ESP_OK) ESP_LOGE(NVS_TAG,"Device is slave, But no host_mac found. \n");
        else 
        {
            ESP_LOGI(NVS_TAG,"Device is slave, history found host_mac is %s. \n", host_mac_str);
            memcpy(get_global_data()->m_host_mac, host_mac_str, 6);
        }
    }
    ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    nvs_close(wificfg_nvs_handler);                     /* 关闭 */
}

void erase_nvs(void)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    // 擦除所有NVS数据
    ESP_ERROR_CHECK(nvs_flash_erase());

    // 重新初始化NVS
    ESP_ERROR_CHECK(nvs_flash_init());
}

void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    is_nvs_init = true;
}

void set_nvs_info(const char *tag, const char *value)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    nvs_handle wificfg_nvs_handler;
    ESP_ERROR_CHECK(nvs_open(NVS_HANDLER, NVS_READWRITE, &wificfg_nvs_handler));
    ESP_ERROR_CHECK(nvs_set_str(wificfg_nvs_handler, tag, value));
    ESP_ERROR_CHECK(nvs_commit(wificfg_nvs_handler));
    nvs_close(wificfg_nvs_handler);

    ESP_LOGI(NVS_TAG,"Save NVS %s to %s successfully. \n", tag, value);
}

void set_nvs_info_uint8_t(const char *tag, uint8_t value)
{
    char value_str[20];
    snprintf(value_str, sizeof(value_str), "%d", value);
    set_nvs_info(tag,value_str);
}

void set_nvs_info_set_host_mac(uint8_t value[6])
{
    // 直接存储6字节MAC地址
    set_nvs_info("host_mac", (const char*)value);
}

void set_nvs_info_set_slave_mac(uint8_t slave_num, uint8_t* value)
{
    // 分配内存：1字节从机数量 + N个从机的MAC地址(每个6字节)
    uint8_t data_size = 1 + (slave_num * 6);
    uint8_t data [data_size];
    // 存储从机数量
    data[0] = slave_num;
    // 存储MAC地址
    memcpy(&data[1], value, slave_num * 6);
    // 写入NVS
    set_nvs_info("slave_mac", (const char*)data);
}