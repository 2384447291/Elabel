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

    init_language_nvs();

    nvs_handle wificfg_nvs_handler; /* 定义一个NVS操作句柄 */
    ESP_ERROR_CHECK(nvs_open(NVS_HANDLER, NVS_READWRITE, &wificfg_nvs_handler) );//打开一个名叫"Elabel_cfg"的可读可写nvs空间
    //--------------------------从nvs中获取reset_count--------------------------------//
    size_t len;                   
    len = sizeof(get_global_data()->reset_count);  
    esp_err_t reset_count_err = nvs_get_u8(wificfg_nvs_handler,"reset_count",&get_global_data()->reset_count) ;
    if(reset_count_err != ESP_OK) ESP_LOGE(NVS_TAG,"No history reset_count found. \n");
    else ESP_LOGI(NVS_TAG,"history reset_count found : %d. \n", get_global_data()->reset_count);

    //--------------------------从nvs中获取wifi_ssid--------------------------------//                  
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

    //--------------------------从nvs中获取is_host--------------------------------//
    len = 20;     
    char is_host_str[len];
    esp_err_t is_host_err = nvs_get_str(wificfg_nvs_handler,"is_host",is_host_str,&len) ;
    if(is_host_err != ESP_OK) ESP_LOGE(NVS_TAG,"No is_host found. \n");
    else 
    {
        ESP_LOGI(NVS_TAG,"history is_host found : %s. \n", is_host_str);
        //如果is_host_str为"0"，则设置为0，否则设置为1
        if(strcmp(is_host_str,"01") == 0)
        {
            get_global_data()->m_is_host = 1;
        }
        else if(strcmp(is_host_str,"02") == 0)
        {
            get_global_data()->m_is_host = 2;
        }
        else
        {
            get_global_data()->m_is_host = 0;
        }
    }

    //--------------------------从nvs中获取slave_mac--------------------------------//
    if(get_global_data()->m_is_host == 1)
    {
        //如果判断为主机，则加载从机地址
        len = 100;
        char slave_mac_str[len];
        esp_err_t slave_mac_err = nvs_get_str(wificfg_nvs_handler,"slave_mac",slave_mac_str,&len);
        if(slave_mac_err != ESP_OK) ESP_LOGE(NVS_TAG,"Device is host, But no slave_mac found. \n");
        else 
        {
            uint8_t slave_mac[len];
            get_nvs_info_uint8_t_array(slave_mac_str, slave_mac);

            get_global_data()->m_slave_num = slave_mac[0];
            for(int i = 0; i < get_global_data()->m_slave_num; i++)
            {
                memcpy(get_global_data()->m_slave_info[i].mac, &slave_mac[i*6+1], 6);
            }

            ESP_LOGI(NVS_TAG,"Device is host, history found slave_num is %d.", get_global_data()->m_slave_num);
            for(int i = 0; i < get_global_data()->m_slave_num; i++)
            {
                ESP_LOGI(NVS_TAG,"Slave %d mac is " MACSTR, i, MAC2STR(get_global_data()->m_slave_info[i].mac));
            }
        }
    }

    //--------------------------从nvs中获取host_mac--------------------------------//
    else if(get_global_data()->m_is_host == 2)
    {
        //如果判断为从机，则加载主机地址
        len = 150;
        char host_message_str[len];
        esp_err_t host_message_err = nvs_get_str(wificfg_nvs_handler,"host_message",host_message_str,&len) ;
        if(host_message_err != ESP_OK) ESP_LOGE(NVS_TAG,"Device is slave, But no host_message found. \n");
        else 
        {
            uint8_t host_message[len];
            get_nvs_info_uint8_t_array(host_message_str, host_message);
            // 读取主机mac地址
            memcpy(get_global_data()->m_host_mac, host_message, 6);
            // 读取主机通道
            get_global_data()->m_host_channel = host_message[6];
            // 读取主机用户名
            int username_len = host_message[7];
            char username[username_len+1];
            memcpy(username, &host_message[8], username_len);
            username[username_len] = '\0';
            memcpy(get_global_data()->m_userName, username, username_len+1);
            
            ESP_LOGI(NVS_TAG,"Device is slave, history found host_mac is " MACSTR ", host_channel is %d, username is %s. \n", 
                MAC2STR(get_global_data()->m_host_mac), get_global_data()->m_host_channel, get_global_data()->m_userName);
        }
    }

    ESP_ERROR_CHECK(nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    nvs_close(wificfg_nvs_handler);                     /* 关闭 */
}

void erase_nvs(void)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    
    nvs_handle wificfg_nvs_handler;
    esp_err_t err = nvs_open(NVS_HANDLER, NVS_READWRITE, &wificfg_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }
    
    // 只擦除 Elabel_cfg 命名空间下的所有数据
    err = nvs_erase_all(wificfg_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error erasing NVS namespace: %s", esp_err_to_name(err));
        nvs_close(wificfg_nvs_handler);
        return;
    }
    
    // 提交更改
    err = nvs_commit(wificfg_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error committing NVS changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(NVS_TAG, "Erase Elabel_cfg namespace successfully. \n");
    }
    
    nvs_close(wificfg_nvs_handler);
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

void set_reset_count(uint8_t reset_count)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    nvs_handle wificfg_nvs_handler;
    ESP_ERROR_CHECK(nvs_open(NVS_HANDLER, NVS_READWRITE, &wificfg_nvs_handler));
    ESP_ERROR_CHECK(nvs_set_u8(wificfg_nvs_handler, "reset_count", reset_count));
    ESP_ERROR_CHECK(nvs_commit(wificfg_nvs_handler));
    nvs_close(wificfg_nvs_handler);

    ESP_LOGI(NVS_TAG,"Save NVS reset_count to %d successfully. \n", reset_count);    
}

//对于输入数组 {0x01, 0x02, 0x03, 0x04}，会正确生成字符串 "01 02 03 04"
void set_nvs_info_uint8_t_array(const char *tag, uint8_t* value, int length)
{
    char value_str[length*3+1];
    int offset = 0;
    
    for(int i = 0; i < length; i++)
    {
        offset += snprintf(value_str + offset, sizeof(value_str) - offset, "%02X ", value[i]);
    }
    
    // 移除最后一个空格
    if (offset > 0 && value_str[offset - 1] == ' ') {
        value_str[offset - 1] = '\0';
    }
    
    set_nvs_info(tag, value_str);
}

void get_nvs_info_uint8_t_array(const char *value_str, uint8_t* value)
{
    int index = 0;
    char hex[3] = {0};
    
    while (value_str[index * 3] != '\0' && value_str[index * 3 + 1] != '\0') 
    {
        hex[0] = value_str[index * 3];
        hex[1] = value_str[index * 3 + 1];
        hex[2] = '\0';
        
        unsigned int byte_val;
        sscanf(hex, "%x", &byte_val);
        value[index] = (uint8_t)byte_val;
        
        index++;
    }
}

void set_nvs_info_set_host_message(uint8_t host_mac[6], uint8_t host_channel, char username[100])
{
    uint8_t data_len = 6 + 1 + strlen(username);
    uint8_t data[data_len];
    // 直接存储6字节MAC地址
    memcpy(data, host_mac, 6);
    data[6] = host_channel;
    data[7] = strlen(username);
    memcpy(data + 8, username, strlen(username));
    set_nvs_info_uint8_t_array("host_message", data, data_len);
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
    set_nvs_info_uint8_t_array("slave_mac", data, data_size);
}

void reset_elabel()
{
    erase_nvs();
    esp_restart();
}

// Language NVS functions
void get_language_nvs_info(char *language)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }

    nvs_handle language_nvs_handler;
    esp_err_t err = nvs_open("language", NVS_READONLY, &language_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening language NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // 从NVS中获取language值
    size_t len = 10; // 假设语言代码最大长度为10
    char language_value[len];
    esp_err_t language_err = nvs_get_str(language_nvs_handler, "m_language", language_value, &len);

    
    
    if(language_err != ESP_OK) {
        ESP_LOGE(NVS_TAG,"No language found in NVS. \n");
    } else {
        ESP_LOGI(NVS_TAG,"Language found: %s. \n", language_value);
        strcpy(language, language_value);
    }

    nvs_close(language_nvs_handler);
}

void set_language_nvs_info(const char *language)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    
    nvs_handle language_nvs_handler;
    esp_err_t err = nvs_open("language", NVS_READWRITE, &language_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening language NVS handle: %s", esp_err_to_name(err));
        return;
    }
    
    err = nvs_set_str(language_nvs_handler, "m_language", language);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error setting language: %s", esp_err_to_name(err));
        nvs_close(language_nvs_handler);
        return;
    }
    
    err = nvs_commit(language_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error committing language changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(NVS_TAG,"Language saved to NVS: %s. \n", language);
    }
    
    nvs_close(language_nvs_handler);
}

//这里和上面的resetnumber一样一般log不会跑到Language not found. Initialize to %s and write to NVS.因为所有烧代码都会跑两次才会到log阶段
void init_language_nvs(void)
{
    if(!is_nvs_init)
    {
        ESP_LOGE(NVS_TAG,"NVS is not initialized. \n");
        return;
    }
    
    nvs_handle language_nvs_handler;
    esp_err_t err = nvs_open("language", NVS_READWRITE, &language_nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error opening language NVS handle: %s", esp_err_to_name(err));
        return;
    }

    char language_value[sizeof(get_global_data()->m_device_info.language)] = {0};
    size_t len = sizeof(language_value);
    err = nvs_get_str(language_nvs_handler, "m_language", language_value, &len);

    if (err != ESP_OK) 
    {
        strcpy(get_global_data()->m_device_info.language, LANGUAGE);
        ESP_LOGE(NVS_TAG, "Language not found. Initialize to %s and write to NVS. \n", LANGUAGE);
        esp_err_t set_err = nvs_set_str(language_nvs_handler, "m_language", LANGUAGE);
        if (set_err == ESP_OK) 
        {
            (void)nvs_commit(language_nvs_handler);
        } 
        else 
        {
            ESP_LOGE(NVS_TAG, "Error setting language to NVS: %s", esp_err_to_name(set_err));
        }
    } 
    else 
    {
        strncpy(get_global_data()->m_device_info.language, language_value, sizeof(get_global_data()->m_device_info.language) - 1);
        get_global_data()->m_device_info.language[sizeof(get_global_data()->m_device_info.language) - 1] = '\0';
        ESP_LOGI(NVS_TAG, "Language loaded from NVS: %s. \n", get_global_data()->m_device_info.language);
    } 

    nvs_close(language_nvs_handler);
}