#include <esp_http_client.h>
#include "cJSON.h"
#include "esp_log.h"
#include "global_message.h"
#include "global_time.h"
#include "global_tool.h"
#include "cJSON.h"
#include "http.h"

#define MUSIC_BLOCK 1024
char music_buffer[MUSIC_BLOCK];

#define JSON_HEADER_SIZE 64
char json_header[JSON_HEADER_SIZE] = {0};  // 用于拼接前2-4K部分数据（足以拿到code）
int json_header_len = 0;

bool dealing_with_music = false;

char m_boundary[20]; // Ensure enough size for boundary
void generate_boundary(char *boundary, size_t size) {
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL));
    snprintf(boundary, size, "------------------------");
    for (size_t i = 0; i < 8; ++i) {
        boundary[size - 9 + i] = chars[rand() % strlen(chars)];
    }
    boundary[size - 1] = '\0';
}

esp_err_t http_post_music_data()
{
    esp_http_client_handle_t client = *get_client();
    esp_err_t err = ESP_FAIL;

    // 1. 设置请求方法
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/device/savePower");

    // 2. 设置 Content-Type
    char content_type[100]; // 确保大小足够
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // 2. 设置 userToken
    esp_http_client_set_header(client, "userToken", get_global_data()->m_usertoken);

    // 4. 生成 boundary 并设置头
    generate_boundary(m_boundary, sizeof(m_boundary));
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/device/savePower");

    //5. 打开音频文件
    FILE *play_file = fopen("/fat/mic.raw", "rb");
    if (play_file == NULL) {
        ESP_LOGE("fuck", "无法打开音频文件");
        return ESP_FAIL;
    }
    fseek(play_file, 0, SEEK_END);
    size_t size = ftell(play_file);
    fseek(play_file, 0, SEEK_SET);
    ESP_LOGI("fuck", "准备上传%d字节的音频数据", size);

    //6. 构造请求体
    char partbegin[256] ,partend[256];
    int len1 = snprintf(partbegin, sizeof(partbegin),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"userOpenId\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"data\"\r\n\r\n",
        m_boundary, get_global_data()->m_usertoken, m_boundary);

    int len2 = snprintf(partend, sizeof(partend),
        "\r\n--%s--\r\n",
        m_boundary);

    // 总长度
    int total_len = len1 + len2 + size;
        
    // 9. 打开连接，write_len = -1 表示使用 chunked 模式
    err = esp_http_client_open(client, total_len);
    if (err != ESP_OK) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // 10. 写入partbegin
    esp_http_client_write(client, partbegin, len1);

    size_t bytes_written = 0;
    // 11. 写入音频数据
    while(dealing_with_music)
    {
        size_t bytes_to_play = 0;
        bytes_to_play = fread(music_buffer, 1, MUSIC_BLOCK, play_file);
        esp_http_client_write(client, music_buffer, bytes_to_play);
        if(bytes_to_play == 0) break; 
        //每传输大于10%打印一次
        progress_update(bytes_written, bytes_written + bytes_to_play, size);
        bytes_written += bytes_to_play;
    }
    ESP_LOGI("music", "传输了%d/%d, 完成了%f", bytes_written, size, (float)bytes_written / size * 100);
    // 12. 写入partend
    esp_http_client_write(client, partend, len2);

    // 13. 获取响应头
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) 
    {
        ESP_LOGE("fuck", "HTTP client fetch headers failed");
        return ESP_FAIL;
    } 

    char response[256];
    int data_read = esp_http_client_read_response(client, response, 256);
    if (data_read >= 0) 
    {
        // ESP_LOGI(HTTP_TAG, "Full response: %s", response);
        // 解析 JSON
        cJSON *json = cJSON_Parse(response);
        if (json == NULL) {
            // 释放 JSON 对象
            cJSON_Delete(json);
            ESP_LOGE(HTTP_TAG, "Full response: %s", response);
            ESP_LOGE("HTTP","JSON parse error!\n");
            return ESP_FAIL;
        }


        // 获取 code 字段
        cJSON *code = cJSON_GetObjectItem(json, "code");
        if (cJSON_IsNumber(code)) 
        {
            if(code->valueint != 200)
            {
                // 释放 JSON 对象
                cJSON_Delete(json);
                ESP_LOGE(HTTP_TAG, "Full response: %s", response);
                return ESP_FAIL;
            }
            else
            {
                return ESP_OK;
            }
        }
    }

    return ESP_FAIL;
}

esp_err_t http_get_music_data()
{
    esp_http_client_handle_t client = *get_client();
    esp_err_t err = ESP_FAIL;
    // 1. 设置请求方法
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/device/savePower");
    // 2. 设置 Content-Type
    char content_type[100]; // 确保大小足够
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);
    // 2. 设置 userToken
    esp_http_client_set_header(client, "userToken", get_global_data()->m_usertoken);

    if (err != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // 3. 生成 boundary 并设置头
    generate_boundary(m_boundary, sizeof(m_boundary));
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/device/savePower");

    // 4. 打开音频文件
    FILE *play_file = fopen("/fat/mic.raw", "rb");
    if (play_file == NULL) {
        ESP_LOGE("fuck", "无法打开音频文件");
        return ESP_FAIL;
    }

    // 5. 构造请求体
    char body[256];
    snprintf(body, sizeof(body),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"focus\"\r\n\r\n"
        "%s\r\n"
        "--%s--\r\n",
        m_boundary, get_global_data()->m_usertoken, m_boundary, m_boundary, m_boundary);

    // 6. 打开连接，write_len = -1 表示使用 chunked 模式
    err = esp_http_client_open(client, strlen(body));
    if (err != ESP_OK) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // 7. 写入请求体
    esp_http_client_write(client, body, strlen(body));
    
    // 8. 获取响应头
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) 
    {
        ESP_LOGE("fuck", "HTTP client fetch headers failed");
        return ESP_FAIL;
    } 

    // 9. 读取响应体
    // while (1) 
    // {
    //     int data_read = esp_http_client_read_response(client, music_buffer, 1);
    //     if (data_read <= 0) break; 

    //     if (json_header_len + data_read < sizeof(json_header))
    //     {
    //         memcpy(json_header + json_header_len, temp_buffer, read_len);
    //     }

    //         cJSON *json = cJSON_Parse(music_buffer);
    //         if (json == NULL) {
    //             // 释放 JSON 对象
    //             cJSON_Delete(json);
    //             ESP_LOGE(HTTP_TAG, "Full response: %s", music_buffer);
    //             ESP_LOGE("HTTP","JSON parse error!\n");
    //             return ESP_FAIL;
    //         }
    //         else{
    //             // ESP_LOGI("HTTP", "Start parse response\n");
    //         };

    //         // 获取 msg 字段
    //         cJSON *msg = cJSON_GetObjectItem(json, "msg");
    //         if (cJSON_IsString(msg) && (msg->valuestring != NULL)) {
    //             // ESP_LOGI("HTTP", "Message: %s", msg->valuestring);
    //         }

    //         // 获取 code 字段
    //         cJSON *code = cJSON_GetObjectItem(json, "code");
    //         if (cJSON_IsNumber(code)) {
    //             // ESP_LOGI("HTTP", "Code: %d", code->valueint);
    //             if(code->valueint != 200)
    //             {
    //                 // 释放 JSON 对象
    //                 cJSON_Delete(json);
    //                 ESP_LOGE(HTTP_TAG, "Full response: %s", response);
    //                 return ESP_FAIL;
    //             }
    //         }
    //         return ESP_OK;
    //     } 
    return ESP_FAIL;
}

void post_music_task(void *pvParameters)
{
    set_need_deal_with_music(true);
    //等待http任务队列空闲
    while(*get_m_http_state() != send_waiting)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    //允许http_音频运行
    dealing_with_music = true;

    //尝试5次上传，直到上传成功  
    int retry_count = 0;
    while (retry_count < 5) 
    {
        if (http_post_music_data() == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "MUSIC Post request success");
            break;
        }
        ESP_LOGE(HTTP_TAG, "MUSIC Post request failed (attempt %d/%d)", retry_count + 1, 5);
        retry_count++;
    }

    set_need_deal_with_music(false);
    vTaskDelete(NULL);
}

void get_music_task(void *pvParameters)
{
    set_need_deal_with_music(true);
    //等待http任务队列空闲
    while(*get_m_http_state() != send_waiting)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    //允许http_音频运行
    dealing_with_music = true;

    //尝试5次上传，直到上传成功  
    int retry_count = 0;
    while (retry_count < 5) 
    {
        if (http_get_music_data() == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "MUSIC Get request success");
            break;
        }
        ESP_LOGE(HTTP_TAG, "MUSIC Get request failed (attempt %d/%d)", retry_count + 1, 5);
        retry_count++;
    }

    set_need_deal_with_music(false);
    vTaskDelete(NULL);
}


void start_post_music(void)
{
    xTaskCreate(post_music_task, "post_music_task", 2048, NULL, 5, NULL);
}

void start_get_music(void)
{
    xTaskCreate(get_music_task, "get_music_task", 2048, NULL, 5, NULL);
}

//阻塞直到任务完结
void stop_music_task(void)
{
    dealing_with_music = false;
    while(get_need_deal_with_music() == false)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
