#include <esp_http_client.h>
#include "cJSON.h"
#include "esp_log.h"
#include "global_message.h"
#include "global_time.h"
#include "global_tool.h"
#include "cJSON.h"
#include "http.h"
#include "httpmusic.h"
#include "http_send.h"

TaskHandle_t get_music_task_handle = NULL;
TaskHandle_t post_music_task_handle = NULL;
bool success_post_music = false;
bool success_get_music = false;

void set_success_post_music(bool _success_post_music)
{
    success_post_music = _success_post_music;
}

void set_success_get_music(bool _success_get_music)
{
    success_get_music = _success_get_music;
}

#define MUSIC_BLOCK 1024
char music_buffer[MUSIC_BLOCK];

#define JSON_HEADER_SIZE 64
char json_header[JSON_HEADER_SIZE] = {0};  // 用于拼接前2-4K部分数据（足以拿到code）
int json_header_len = 0;

bool dealing_with_music = false;

struct get_music_param
{
    int32_t http_music_unique_id;
    uint32_t* ptr_mcodec_record_message_unique_id;
    uint32_t success_record_message_unique_id;
};

esp_err_t http_post_music_data(int32_t id)
{
    char m_boundary[20];
    generate_boundary(m_boundary, sizeof(m_boundary));

    esp_http_client_handle_t client = *get_client();
    esp_err_t err = ESP_FAIL;
    esp_err_t result = ESP_FAIL;
    bool client_opened = false;
    FILE *play_file = NULL;
    cJSON *json = NULL;

    // 1. 设置请求方法
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,HTTP_URL"/userApi/todo/addAudio");

    // 2. 设置 Content-Type
    char content_type[100]; // 确保大小足够
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // 3. 设置 userToken
    esp_http_client_set_header(client, "userToken", get_global_data()->m_usertoken);

    //4. 打开音频文件只可读
    play_file = fopen("/fat/mic.raw", "rb");
    if (play_file == NULL) {
        ESP_LOGE("fuck", "无法打开音频文件");
        goto cleanup;
    }
    fseek(play_file, 0, SEEK_END);
    size_t size = ftell(play_file);
    fseek(play_file, 0, SEEK_SET);
    ESP_LOGI("music", "准备上传%d字节的音频数据", size);

    //5. 构造请求体
    char partbegin[512] ,partend[512];
    int len1 = snprintf(partbegin, sizeof(partbegin),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
        "%ld\r\n"

        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audioInfo\"; filename=\"audioInfo.raw\" \r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Transfer-Encoding: 8bit\r\n\r\n",
        m_boundary, id, m_boundary);

    int len2 = snprintf(partend, sizeof(partend),
        "\r\n--%s--\r\n",
        m_boundary);

    // 总长度
    int total_len = len1 + len2 + size;
        
    // 6. 打开连接，write_len = -1 表示使用 chunked 模式
    err = esp_http_client_open(client, total_len);
    if (err != ESP_OK) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    client_opened = true;

    // 7. 写入partbegin
    if (esp_http_client_write(client, partbegin, len1) < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to write multipart header");
        goto cleanup;
    }

    size_t bytes_written = 0;
    // 8. 写入音频数据
    while(dealing_with_music)
    {
        size_t bytes_to_play = 0;
        bytes_to_play = fread(music_buffer, 1, MUSIC_BLOCK, play_file);
        if (bytes_to_play > 0 && esp_http_client_write(client, music_buffer, bytes_to_play) < 0) {
            ESP_LOGE(HTTP_TAG, "Failed to write music payload");
            goto cleanup;
        }
        if(bytes_to_play == 0) break; 
        //每传输大于10%打印一次
        progress_update(bytes_written, bytes_written + bytes_to_play, size);
        bytes_written += bytes_to_play;
    }
    // 9. 写入partend
    if (esp_http_client_write(client, partend, len2) < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to write multipart tail");
        goto cleanup;
    }

    // 10. 获取响应头
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) 
    {
        ESP_LOGE("music", "HTTP client fetch headers failed");
        goto cleanup;
    } 

    char response[256];
    int data_read = esp_http_client_read_response(client, response, sizeof(response) - 1);
    if (data_read >= 0) 
    {
        response[data_read] = '\0';
        // ESP_LOGI(HTTP_TAG, "Full response: %s", response);
        // 解析 JSON
        json = cJSON_Parse(response);
        if (json == NULL) {
            ESP_LOGE(HTTP_TAG, "Full response: %s", response);
            ESP_LOGE("HTTP","JSON parse error!\n");
            goto cleanup;
        }


        // 获取 code 字段
        cJSON *code = cJSON_GetObjectItem(json, "code");
        if (cJSON_IsNumber(code)) 
        {
            if(code->valueint != 200)
            {
                ESP_LOGE(HTTP_TAG, "Full response: %s", response);
                goto cleanup;
            }
            else
            {
                ESP_LOGI("music", "传输给任务%ld, 了%d/%d, 完成了%f", id, bytes_written, size, (float)bytes_written / size * 100);
                result = ESP_OK;
                goto cleanup;
            }
        }
    }

cleanup:
    if (json != NULL) {
        cJSON_Delete(json);
    }
    if (play_file != NULL) {
        fclose(play_file);
    }
    if (client_opened) {
        esp_http_client_close(client);
    }
    return result;
}

esp_err_t http_get_music_data(int32_t id, uint32_t* ptr_mcodec_record_message_unique_id, uint32_t success_record_message_unique_id)
{
    char m_boundary[20];
    generate_boundary(m_boundary, sizeof(m_boundary));

    esp_http_client_handle_t client = *get_client();
    esp_err_t err = ESP_FAIL;
    esp_err_t result = ESP_FAIL;
    bool client_opened = false;
    FILE *play_file = NULL;

    // 1. 设置请求方法
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_url(client,HTTP_URL"/userApi/todo/getAudio");

    // 2. 设置 Content-Type
    char content_type[100]; // 确保大小足够
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // 3. 设置 userToken
    esp_http_client_set_header(client, "userToken", get_global_data()->m_usertoken);

    // 4. 打开音频文件只可写
    play_file = fopen("/fat/mic.raw", "wb");
    if (play_file == NULL) {
        ESP_LOGE("fuck", "无法打开音频文件");
        goto cleanup;
    }

    // 5. 构造请求体
    char body[256];
    snprintf(body, sizeof(body),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
        "%ld\r\n"

        "--%s--\r\n",
        m_boundary, id, m_boundary);

    // 6. 打开连接，write_len = -1 表示使用 chunked 模式
    err = esp_http_client_open(client, strlen(body));
    if (err != ESP_OK) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    client_opened = true;

    // 7. 写入请求体
    if (esp_http_client_write(client, body, strlen(body)) < 0) {
        ESP_LOGE(HTTP_TAG, "Failed to write getAudio request body");
        goto cleanup;
    }
    
    // 8. 获取响应头
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) 
    {
        ESP_LOGE("fuck", "HTTP client fetch headers failed");
        goto cleanup;
    } 
    size_t total_bytes_write = 0;
    // 9. 读取响应体
    while (1) 
    {
        memset(music_buffer, 0, MUSIC_BLOCK);
        int data_read = esp_http_client_read_response(client, music_buffer, MUSIC_BLOCK);
        if(data_read == 0 && total_bytes_write == 0)
        {
            ESP_LOGE("fuck", "错误的请求");
            goto cleanup;
        }
        if (data_read <= 0) break; 
        fwrite(music_buffer, 1, data_read, play_file);
        total_bytes_write += data_read;
    }
    ESP_LOGI("music", "接收到了%d字节的音频数据", total_bytes_write);
    //如果是接收端收到全部数据才赋值
    *ptr_mcodec_record_message_unique_id = success_record_message_unique_id;
    result = ESP_OK;

cleanup:
    if (play_file != NULL) {
        fclose(play_file);
    }
    if (client_opened) {
        esp_http_client_close(client);
    }
    return result;
}

void post_music_task(void *pvParameters)
{
    int32_t http_music_unique_id = *(int32_t*)pvParameters;

    set_need_deal_with_music(true);
    //等待http任务队列空闲
    while(*get_m_http_state() != send_waiting)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    //允许http_音频运行
    dealing_with_music = true;
    //尝试3次上传，直到上传成功  
    int retry_count = 0;
    while (retry_count < 3) 
    {
        if (http_post_music_data(http_music_unique_id) == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "MUSIC Post request success");
            break;
        }
        ESP_LOGE(HTTP_TAG, "MUSIC Post request failed (attempt %d/%d)", retry_count + 1, 3);
        retry_count++;
    }
    if (*get_client() != NULL) {
        esp_http_client_cleanup(*get_client());
    }
    *get_client() = esp_http_client_init(get_config());
    if (*get_client() == NULL) {
        ESP_LOGE(HTTP_TAG, "Re-init HTTP client failed after post music");
    }
    dealing_with_music = false;
    set_need_deal_with_music(false);
    post_music_task_handle = NULL;
    vTaskDelete(NULL);
}

void get_music_task(void *pvParameters)
{
    struct get_music_param get_music_param = *(struct get_music_param*)pvParameters;

    set_need_deal_with_music(true);
    //等待http任务队列空闲
    while(*get_m_http_state() != send_waiting)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    //允许http_音频运行
    dealing_with_music = true;

    //尝试3次上传，直到上传成功  
    int retry_count = 0;
    while (retry_count < 3) 
    {
        if (http_get_music_data(get_music_param.http_music_unique_id, 
        get_music_param.ptr_mcodec_record_message_unique_id,
        get_music_param.success_record_message_unique_id) == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "MUSIC Get request success");
            break;
        }
        ESP_LOGE(HTTP_TAG, "MUSIC Get request failed (attempt %d/%d)", retry_count + 1, 3);
        retry_count++;
    }
    if (*get_client() != NULL) {
        esp_http_client_cleanup(*get_client());
    }
    *get_client() = esp_http_client_init(get_config());
    if (*get_client() == NULL) {
        ESP_LOGE(HTTP_TAG, "Re-init HTTP client failed after get music");
    }
    dealing_with_music = false;
    set_need_deal_with_music(false);
    get_music_task_handle = NULL;
    vTaskDelete(NULL);
}


void start_post_music(int32_t _music_unique_id)
{
    if(post_music_task_handle != NULL || success_post_music)
    {
        return;
    }

    static int32_t http_music_unique_id;
    http_music_unique_id = _music_unique_id;
    xTaskCreate(post_music_task, "post_music_task", 5120, &http_music_unique_id, 5, &post_music_task_handle);
}

void start_get_music(int32_t _music_unique_id, uint32_t* _ptr_mcodec_record_message_unique_id, uint32_t success_record_message_unique_id)
{
    if(get_music_task_handle != NULL || success_get_music)
    {
        return;
    }
    static struct get_music_param get_music_param;
    get_music_param.http_music_unique_id = _music_unique_id;
    get_music_param.ptr_mcodec_record_message_unique_id = _ptr_mcodec_record_message_unique_id;
    get_music_param.success_record_message_unique_id = success_record_message_unique_id;
    xTaskCreate(get_music_task, "get_music_task", 5120, &get_music_param, 5, &get_music_task_handle);
}

