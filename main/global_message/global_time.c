#include "global_time.h"
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "cJSON.h"

#undef ESP_LOGI
#define ESP_LOGI(tag, format, ...) 

bool is_syset_time = false;
time_t now = 0;
struct tm timeinfo = {0};
uint32_t elabelUpdateTick = 0;
//--------------------------------------SNTP时间同步函数-----------------------------------------//
// void initialize_sntp(void)
// {
//     ESP_LOGI("UNIX TIME", "Initializing SNTP");
//     esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
//     sntp_setservername(0, "cn.pool.ntp.org");
//     sntp_setservername(1, "210.72.145.44");		// 国家授时中心服务器 IP 地址
//     sntp_setservername(2, "ntp.aliyun.com");   

//     #ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
//         sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
//     #endif
//     esp_sntp_init();
// }

// void inner_syset()
// {
//     initialize_sntp();

//     int retry = 0;

//     // 等待时间同步完成
//     while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 200) {
//         vTaskDelay(5000 / portTICK_PERIOD_MS);
//         ESP_LOGE("SNTP_SYTIME", "Waiting for system time to be set... (%d/%d)", retry, 200);
//     }

//     ESP_LOGI("UNIX TIME", "System time synchronized successfully.");
//     is_syset_time = true;
//     vTaskDelete(NULL);
//     // wait for time to be set
// }

// void obtain_time(void *pvParameter)
// {
//     inner_syset();
// }

// void SNTP_syset_time(void)
// {
//     xTaskCreate(&obtain_time, "obtain_time", 4096, NULL, 10, NULL);
// }
//--------------------------------------SNTP时间同步函数------------------------------------------//




//--------------------------------------http时间同步函数-----------------------------------------//
// 定义一个静态缓冲区来存储接收的数据
char *response_buffer = NULL;
int response_buffer_len = 0;
bool send_error = false;

#define URL "http://mshopact.vivo.com.cn/tool/config"
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("HTTP_SYTIME", "Get_Sys_time_HTTP_EVENT_ERROR");
            send_error = true;
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI("HTTP_SYTIME", "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI("HTTP_SYTIME", "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            response_buffer = realloc(response_buffer, response_buffer_len + evt->data_len + 1);
            if (response_buffer == NULL) {
                ESP_LOGI("HTTP_SYTIME", "Failed to allocate memory for response buffer");
                return ESP_ERR_NO_MEM;
            }
            memcpy(response_buffer + response_buffer_len, evt->data, evt->data_len);
            response_buffer_len += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("HTTP_SYTIME", "HTTP_EVENT_ON_FINISH");
            if (response_buffer == NULL) break;
            ESP_LOGI("HTTP_SYTIME", "Full response: %s", response_buffer);
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) 
            {
                cJSON *data = cJSON_GetObjectItem(json, "data");
                if (data) 
                {
                    //校准时间
                    long long nowTime = (long long)cJSON_GetObjectItem(data, "nowTime")->valuedouble;
                    struct timeval tv;
                    
                    // 将毫秒时间戳转换为秒和微秒
                    tv.tv_sec = nowTime / 1000;          // 秒
                    tv.tv_usec = (nowTime % 1000) * 1000; // 微秒

                    // 设置系统时间
                    if (settimeofday(&tv, NULL) < 0) {
                        perror("settimeofday");
                    }
                    // Set timezone to China Standard Time
                    setenv("TZ", "CST-8", 1);
                    tzset();
                    time(&now);
	                localtime_r(&now, &timeinfo);
                    is_syset_time = true;
                }
            }
            // 这里可以对完整的响应数据进行处理
            free(response_buffer); // 释放内存
            response_buffer = NULL;
            response_buffer_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI("HTTP_SYTIME", "HTTP_EVENT_ON_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void HTTP_syset_time(void)
{
    is_syset_time = false;

    send_error = false;

    esp_http_client_config_t sys_time_config = {
        .url = URL,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t sys_time_client = esp_http_client_init(&sys_time_config);

    esp_err_t err = esp_http_client_perform(sys_time_client);

    if (err == ESP_OK) {
        ESP_LOGI("HTTP_SYTIME", "HTTPS Status = %d, content_length = %d",
                 esp_http_client_get_status_code(sys_time_client),
                 esp_http_client_get_content_length(sys_time_client));
    } else {
        ESP_LOGE("HTTP_SYTIME", "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    // Cleanup
    esp_http_client_cleanup(sys_time_client); 

    while(!is_syset_time)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if(send_error) HTTP_syset_time();
    }
}
//--------------------------------------http时间同步函数-----------------------------------------//



//-----------------------------------------时间戳获取-------------------------------------------//
long long get_unix_time(void)
{
    while(!is_syset_time) 
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_LOGE("UNIX TIME","Waiting Systime set\n");
    }
    // 获取当前 Unix 时间戳
    struct timeval now;

    gettimeofday(&now, NULL);

    // 将秒和微秒转换为毫秒
    long long timestamp_in_ms = (long long)now.tv_sec * 1000 + now.tv_usec / 1000;
    
    ESP_LOGI("UNIX TIME","Return Unix timestamp in milliseconds: %lld\n", timestamp_in_ms);

	// 打印现在时间
	char strftime_buf[64];
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI("UNIX TIME", "Current time: %s", strftime_buf);

    return timestamp_in_ms;
}

char* get_time_str(void)
{
    HTTP_syset_time();
	// 打印现在时间
	static char strftime_buf[64];
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI("UNIX TIME", "Current time: %s", strftime_buf);
    return strftime_buf;
}
//-----------------------------------------时间戳获取-------------------------------------------//