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

    size_t len;                   
    len = sizeof(get_global_data()->m_wifi_ssid);  
    esp_err_t ssid_err = nvs_get_str(wificfg_nvs_handler,"wifi_ssid",get_global_data()->m_wifi_ssid,&len) ;
    if(ssid_err != ESP_OK) ESP_LOGE(NVS_TAG,"No history wifi_ssid found. \n");
    else ESP_LOGI(NVS_TAG,"history wifi_ssid found : %s. \n", get_global_data()->m_wifi_ssid);

    len = sizeof(get_global_data()->m_wifi_password);      /* 从NVS中获取passwd */
    esp_err_t passwd_err = nvs_get_str(wificfg_nvs_handler,"wifi_passwd",get_global_data()->m_wifi_password,&len) ;
    if(passwd_err != ESP_OK) ESP_LOGE(NVS_TAG,"No history wifi_passwd found. \n");
    else ESP_LOGI(NVS_TAG,"history wifi_passwd found : %s. \n", get_global_data()->m_wifi_password);

    len = sizeof(get_global_data()->m_usertoken);      /* 从NVS中获取customer */
    esp_err_t customer_err = nvs_get_str(wificfg_nvs_handler,"customer",get_global_data()->m_usertoken,&len) ;
    if(customer_err != ESP_OK) ESP_LOGE(NVS_TAG,"No customer found. \n");
    else ESP_LOGI(NVS_TAG,"history customer found : %s. \n", get_global_data()->m_usertoken);

    len = sizeof(get_global_data()->m_userName);      /* 从NVS中获取username */
    esp_err_t username_err = nvs_get_str(wificfg_nvs_handler,"username",get_global_data()->m_userName,&len) ;
    if(username_err != ESP_OK) ESP_LOGE(NVS_TAG,"No username found. \n");
    else ESP_LOGI(NVS_TAG,"history username found : %s. \n", get_global_data()->m_userName);

    len = sizeof(get_global_data()->m_language);      /* 从NVS中获取language */
    char language_str[10];
    esp_err_t language_err = nvs_get_str(wificfg_nvs_handler,"language",language_str,&len) ;
    if(language_err != ESP_OK) ESP_LOGE(NVS_TAG,"No language found. \n");
    else ESP_LOGI(NVS_TAG,"history language found : %s. \n", language_str);
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
