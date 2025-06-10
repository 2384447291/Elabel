#ifndef HTTP_SEND_H
#define HTTP_SEND_H
#include <esp_http_client.h>
#include "cJSON.h"
#include "esp_log.h"
#include "global_message.h"
#include "global_time.h"
#include "cJSON.h"
#include "http.h"
// #undef ESP_LOGI
// #define ESP_LOGI(tag, format, ...) 

void generate_boundary(char *boundary, size_t size);

esp_err_t http_send(http_task_struct* m_task_struct);
#endif