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

void set_region(void)
{
    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
}

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
                        set_region();
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

    if(err != ESP_OK) 
    {
        ESP_LOGE("HTTP_SYTIME", "HTTP GET request failed: %s", esp_err_to_name(err));
        send_error = true;
    }

    esp_http_client_cleanup(sys_time_client); 

    while(!is_syset_time)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if(send_error) HTTP_syset_time();
    }
    Log_time();
}

void EspNow_syset_time(long long nowTime)
{
    ESP_LOGI("HTTP_SYTIME", "nowTime: %lld", nowTime);
    struct timeval tv;
    
    // 将毫秒时间戳转换为秒和微秒
    tv.tv_sec = nowTime / 1000;          // 秒
    tv.tv_usec = (nowTime % 1000) * 1000; // 微秒

    // 设置系统时间
    if (settimeofday(&tv, NULL) < 0) {
        perror("settimeofday");
    }
    
    set_region();
    
    time(&now);
    localtime_r(&now, &timeinfo);
    is_syset_time = true;
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

void get_clock_time(char* str_time)
{
    if(!is_syset_time){
        ESP_LOGE("CLOCK TIME","Systime not set");
        strcpy(str_time, "00:00");
        return;
    }
    // 格式化时间为 HH:MM
    strftime(str_time, 6, "%H:%M", &timeinfo);
}

time_description get_date_time()
{
    time_description time_desc;
    time_desc.year = timeinfo.tm_year + 1900;
    time_desc.month = timeinfo.tm_mon;
    time_desc.day = timeinfo.tm_mday;
    time_desc.week = timeinfo.tm_wday;
    time_desc.hour = timeinfo.tm_hour;
    time_desc.minute = timeinfo.tm_min;
    time_desc.second = timeinfo.tm_sec;
    ESP_LOGI("DATE TIME", "year: %d, month: %d, day: %d, week: %d, hour: %d, minute: %d, second: %d", 
        time_desc.year, time_desc.month, time_desc.day, time_desc.week, time_desc.hour, time_desc.minute, time_desc.second);
    return time_desc;
}

void Log_time(void)
{
	// 打印现在时间
	char strftime_buf[64];
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI("UNIX TIME", "Current time: %s", strftime_buf);
}

void get_open_time(char* str_time)
{
    // 1. 获取微秒级计时（从开机算起）
    int64_t us = esp_timer_get_time();    // 单位：μs
    int64_t secs = us / 1000000;          // 换算成秒（向下取整）

    // 2. 计算天、小时、分钟
    int days    = secs / 86400;                 // 86400 = 24 * 3600
    int hours   = (secs % 86400) / 3600;        // 3600 秒 = 1 小时
    int minutes = (secs % 3600) / 60;           // 60 秒 = 1 分钟

    // 3. 格式化写入到 str_time 中
    //    格式为："<days>D <hours>H <minutes>M"，例如："3D 2H 12M"
    //    假设 str_time 已经分配了足够大的空间（>= 20 字节）。
    sprintf(str_time, "%dD %dH %dM", days, hours, minutes);
}
//-----------------------------------------时间戳获取-------------------------------------------//