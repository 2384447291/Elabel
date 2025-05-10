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

//--------------------------------------http时间同步函数-----------------------------------------//
// 定义一个静态缓冲区来存储接收的数据
char *response_buffer = NULL;
int response_buffer_len = 0;
bool send_error = false;

#define URL "http://120.77.1.151:8080/userApi/common/getTimeStamp"
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
                cJSON *code = cJSON_GetObjectItem(json, "code");
                if (code && code->valueint == 200) 
                {
                    cJSON *data = cJSON_GetObjectItem(json, "data");
                    if (data && data->valuedouble > 0) 
                    {
                        long long nowTime = (long long)data->valuedouble;
                        ESP_LOGI("HTTP_SYTIME", "nowTime: %lld", nowTime);
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
                cJSON_Delete(json);
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
        // ESP_LOGI("HTTP_SYTIME", "HTTPS Status = %d, content_length = %d",
        //          esp_http_client_get_status_code(sys_time_client),
        //          esp_http_client_get_content_length(sys_time_client));
    } else {
        ESP_LOGE("HTTP_SYTIME", "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(sys_time_client); 

    while(!is_syset_time)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if(send_error) HTTP_syset_time();
    }
    Log_time();
}
//--------------------------------------http时间同步函数-----------------------------------------//



//-----------------------------------------时间戳获取-------------------------------------------//
long long get_unix_time(void)
{
    if(!is_syset_time){
        ESP_LOGE("UNIX TIME","Systime not set");
        return 0;
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

void get_unix_time_str(char* str_time, size_t size)
{
    long long timestamp_in_ms = get_unix_time();
    snprintf(str_time, size, "%lld", timestamp_in_ms);
}

void Log_time(void)
{
	// 打印现在时间
	char strftime_buf[64];
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGW("UNIX TIME", "Current time: %s", strftime_buf);
}
//-----------------------------------------时间戳获取-------------------------------------------//