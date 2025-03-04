// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>

#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "esp_wifi.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "esp_timer.h"

#include "espnow.h"
#include "espnow_console.h"
#include "espnow_ctrl.h"
#include "espnow_log.h"
#include "espnow_ota.h"
#include "espnow_prov.h"
#include "espnow_utils.h"

#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <errno.h>
#include <sys/param.h>
#include "driver/gpio.h"

static const char *TAG = "iperf_cmd";


struct espnow_iperf_cfg {
    bool finish;
    uint16_t packet_len;
    uint16_t transmit_time;
    uint32_t ping_count;
    uint16_t report_interval;
    espnow_data_type_t type;
    espnow_frame_head_t frame_head;
    uint8_t addr[6];
    int gpio_num;
} ;

espnow_iperf_cfg g_iperf_cfg = {
    .finish = false,
    .packet_len      = ESPNOW_DATA_LEN,
    .transmit_time   = 60,
    .ping_count      = 64,
    .report_interval = 3,
    .type            = ESPNOW_DATA_TYPE_RESERVED,
    .frame_head = {.broadcast  = false},
    .addr       = {0xa0, 0x85, 0xe3, 0xf6, 0x3e, 0x14},
};

typedef enum {
    IPERF_BANDWIDTH,
    IPERF_BANDWIDTH_STOP,
    IPERF_BANDWIDTH_STOP_ACK,
    IPERF_PING,
    IPERF_PING_ACK,
} iperf_type_t;

typedef struct {
    iperf_type_t type;
    uint32_t seq;
    uint8_t data[0];
} espnow_iperf_data_t;

#define IPERF_QUEUE_SIZE 10
static QueueHandle_t g_iperf_queue = NULL;
typedef struct {
    uint8_t src_addr[6];
    void *data;
    size_t size;
    wifi_pkt_rx_ctrl_t rx_ctrl;
} iperf_recv_data_t;

static esp_err_t espnow_iperf_initiator_recv(uint8_t *src_addr, void *data,
                      size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_LOGI(TAG,"espnow_iperf_initiator_recv");
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    if (g_iperf_queue) {
        iperf_recv_data_t iperf_data = { 0 };
        iperf_data.data = ESP_MALLOC(size);
        if (!iperf_data.data) {
            return ESP_FAIL;
        }
        memcpy(iperf_data.data, data, size);
        iperf_data.size = size;
        memcpy(iperf_data.src_addr, src_addr, 6);
        memcpy(&iperf_data.rx_ctrl, rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
        if (xQueueSend(g_iperf_queue, &iperf_data, 0) != pdPASS) {
            ESP_LOGW(TAG, "[%s, %d] Send iperf recv queue failed", __func__, __LINE__);
            ESP_FREE(iperf_data.data);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static void espnow_iperf_initiator_task(void *arg)
{
    esp_err_t ret   = ESP_OK;
    espnow_iperf_data_t *iperf_data = (espnow_iperf_data_t *)ESP_CALLOC(1, g_iperf_cfg.packet_len);
    iperf_data->type = IPERF_BANDWIDTH;
    iperf_data->seq   = 0;


    int64_t start_time = esp_timer_get_time();
    int64_t end_time = start_time + g_iperf_cfg.transmit_time * 1000 * 1000;
    uint32_t total_count   = 0;

    if (!g_iperf_cfg.frame_head.broadcast) {
        espnow_add_peer(g_iperf_cfg.addr, NULL);
    }

    ESP_LOGI(TAG, "[  Responder MAC  ]   Interval     Transfer     Frame_rate     Bandwidth");

    for (int64_t report_time = start_time + g_iperf_cfg.report_interval * 1000 * 1000, report_count = 0;
            esp_timer_get_time() < end_time && !g_iperf_cfg.finish;) {
        ret = espnow_send(g_iperf_cfg.type, g_iperf_cfg.addr, iperf_data,
                          g_iperf_cfg.packet_len, &g_iperf_cfg.frame_head, portMAX_DELAY);
        ESP_ERROR_CONTINUE(ret != ESP_OK && ret != ESP_ERR_WIFI_TIMEOUT, "<%s> espnow_send", esp_err_to_name(ret));
        iperf_data->seq++;
        ++total_count;

        if (esp_timer_get_time() >= report_time) {
            uint32_t report_time_s = (report_time - start_time) / (1000 * 1000);
            double report_size    = (iperf_data->seq - report_count) * g_iperf_cfg.packet_len / 1e6;

            ESP_LOGI(TAG, "["MACSTR"]  %2d-%2d sec  %2.2f MBytes   %0.2f Hz  %0.2f Mbps",
                     MAC2STR(g_iperf_cfg.addr), report_time_s - g_iperf_cfg.report_interval, report_time_s,
                     report_size, (iperf_data->seq - report_count) * 1.0 / g_iperf_cfg.report_interval, report_size * 8 / g_iperf_cfg.report_interval);

            report_time = esp_timer_get_time() + g_iperf_cfg.report_interval * 1000 * 1000;
            report_count = iperf_data->seq;
        }
    }

    iperf_data->type  = IPERF_BANDWIDTH_STOP;
    int retry_count     = 5;
    wifi_pkt_rx_ctrl_t rx_ctrl = {0};
    uint32_t spend_time_ms = (esp_timer_get_time() - start_time) / 1000;

    do {
        ret = espnow_send(g_iperf_cfg.type, g_iperf_cfg.addr, iperf_data,
                          g_iperf_cfg.packet_len, &g_iperf_cfg.frame_head, portMAX_DELAY);
        ESP_ERROR_CONTINUE(ret != ESP_OK, "<%s> espnow_send", esp_err_to_name(ret));

        memset(iperf_data, 0, g_iperf_cfg.packet_len);
        iperf_recv_data_t recv_data = { 0 };
        if (g_iperf_queue && xQueueReceive(g_iperf_queue, &recv_data, pdMS_TO_TICKS(1000)) == pdPASS) {
            ret = ESP_OK;
            memcpy(iperf_data, recv_data.data, recv_data.size);
            memcpy(g_iperf_cfg.addr, recv_data.src_addr, 6);
            memcpy(&rx_ctrl, &recv_data.rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
            ESP_FREE(recv_data.data);
        } else {
            ret = ESP_FAIL;
        }
    } while (ret != ESP_OK && retry_count-- > 0 && iperf_data->type != IPERF_BANDWIDTH_STOP_ACK);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "<%s> Receive responder response failed", esp_err_to_name(ret));
    } else {
        uint32_t write_count = iperf_data->seq > 0 ? iperf_data->seq - 1 : 0;
        uint32_t lost_count  = total_count - write_count;
        double total_len     = (total_count * g_iperf_cfg.packet_len) / 1e6;

        if (total_count && write_count && spend_time_ms) {
            ESP_LOGI(TAG, "initiator Report:");
            ESP_LOGI(TAG, "[ ID] Interval      Transfer       Bandwidth      Jitter   Lost/Total Datagrams  RSSI  Channel");
            ESP_LOGI(TAG, "[000] %2d-%2d sec    %2.2f MBytes    %0.2f Mbps    %0.2f ms    %d/%d (%0.2f%%)    %d    %d",
                    0, spend_time_ms / 1000, total_len, total_len * 8 * 1000 / spend_time_ms, spend_time_ms * 1.0 / write_count,
                    lost_count, total_count, lost_count * 100.0 / total_count, rx_ctrl.rssi, rx_ctrl.channel);
        }
    }

    if (!g_iperf_cfg.frame_head.broadcast) {
        espnow_del_peer(g_iperf_cfg.addr);
    }

    ESP_FREE(iperf_data);
    g_iperf_cfg.finish = true;

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_RESERVED, 0, NULL);
    if (g_iperf_queue) {
        iperf_recv_data_t tmp_data =  { 0 };

        while (xQueueReceive(g_iperf_queue, &tmp_data, 0)) {
            ESP_FREE(tmp_data.data);
        }

        vQueueDelete(g_iperf_queue);
        g_iperf_queue = NULL;
    }

    vTaskDelete(NULL);
}

static esp_err_t espnow_iperf_responder(uint8_t *src_addr, void *data,
                      size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    esp_err_t ret         = ESP_OK;
    espnow_iperf_data_t *iperf_data   = (espnow_iperf_data_t *)data;
    static int64_t start_time;
    static uint32_t recv_count;
    static int64_t report_time;
    static uint32_t report_count;

    memcpy(g_iperf_cfg.addr, src_addr, 6);

    if (!g_iperf_cfg.finish) {
        recv_count++;

        if (iperf_data->seq == 0) {
            recv_count   = 0;
            start_time  = esp_timer_get_time();
            report_time = start_time + g_iperf_cfg.report_interval * 1000 * 1000;
            report_count = 0;
        }

        if (iperf_data->type == IPERF_BANDWIDTH && esp_timer_get_time() >= report_time) {
            uint32_t report_time_s = (report_time - start_time) / (1000 * 1000);
            double report_size    = (recv_count - report_count) * size / 1e6;
            ESP_LOGI(TAG, "["MACSTR"]  %2d-%2d sec  %2.2f MBytes  %0.2f Mbps  %d dbm",
                     MAC2STR(g_iperf_cfg.addr), report_time_s - g_iperf_cfg.report_interval, report_time_s,
                     report_size, report_size * 8 / g_iperf_cfg.report_interval, rx_ctrl->rssi);

            report_time = esp_timer_get_time() + g_iperf_cfg.report_interval * 1000 * 1000;
            report_count = recv_count;
        } else if (iperf_data->type == IPERF_PING) {
            ESP_LOGV(TAG, "Recv IPERF_PING, seq: %d, recv_count: %d", iperf_data->seq, recv_count);
            iperf_data->type = IPERF_PING_ACK;


            if (!g_iperf_cfg.frame_head.broadcast) {
                espnow_add_peer(g_iperf_cfg.addr, NULL);
            }

            ret = espnow_send(g_iperf_cfg.type, g_iperf_cfg.addr, iperf_data,
                              size, &g_iperf_cfg.frame_head, portMAX_DELAY);

            if (!g_iperf_cfg.frame_head.broadcast) {
                espnow_del_peer(g_iperf_cfg.addr);
            }

            ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> espnow_send", esp_err_to_name(ret));
        } else if (iperf_data->type == IPERF_BANDWIDTH_STOP) {
            uint32_t total_count = iperf_data->seq + 1;
            uint32_t lost_count  = total_count - recv_count;
            double total_len     = (total_count * size) / 1e6;
            uint32_t spend_time_ms  = (esp_timer_get_time() - start_time) / 1000;

            ESP_LOGI(TAG, "[ ID] Interval      Transfer       Bandwidth      Jitter   Lost/Total Datagrams");
            ESP_LOGI(TAG, "[000] %2d-%2d sec    %2.2f MBytes    %0.2f Mbps    %0.2f ms    %d/%d (%0.2f%%)",
                     0, spend_time_ms / 1000, total_len, total_len * 8 * 1000 / spend_time_ms, spend_time_ms * 1.0 / recv_count,
                     lost_count, total_count, lost_count * 100.0 / total_count);

            iperf_data->seq = recv_count;
            iperf_data->type = IPERF_BANDWIDTH_STOP_ACK;
            ESP_LOGD(TAG, "iperf_data->seq: %d",  iperf_data->seq);

            espnow_frame_head_t frame_head = {
                .filter_adjacent_channel = true,
            };

            espnow_add_peer(g_iperf_cfg.addr, NULL);
            ret = espnow_send(g_iperf_cfg.type, g_iperf_cfg.addr, iperf_data,
                              sizeof(espnow_iperf_data_t), &frame_head, portMAX_DELAY);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> espnow_send", esp_err_to_name(ret));

            espnow_del_peer(g_iperf_cfg.addr);
        }
    }

    return ESP_OK;
}

static void espnow_iperf_responder_start(void)
{
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_RESERVED, 1, espnow_iperf_responder);
    ESP_LOGI(TAG, "[  Initiator MAC  ] Interval       Transfer     Bandwidth   RSSI");
}

extern "C" void app_main(void)
{
    espnow_storage_init();

    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

       // ESP-NOW初始化
        espnow_config_t espnow_config = {
            .pmk = {'E','S','P','_','N','O','W',0,0,0,0,0,0,0,0,0},
            .forward_enable = 1,
            .forward_switch_channel = 0,
            .sec_enable = 0,
            .reserved1 = 0,
            .qsize = 32,
            .send_retry_num = 10,
            .send_max_timeout = pdMS_TO_TICKS(3000),
            .receive_enable = {
                .ack = 1,
                .forward = 1,
                .group = 1,
                .provisoning = 0,
                .control_bind = 0,
                .control_data = 0,
                .ota_status = 0,
                .ota_data = 0,
                .debug_log = 0,
                .debug_command = 0,
                .data = 0,
                .sec_status = 0,
                .sec = 0,
                .sec_data = 0,
                .reserved2 = 0
            }
        };
        espnow_init(&espnow_config);
         espnow_iperf_responder_start();
       // 创建性能测试任务
        // xTaskCreate(espnow_iperf_initiator_task, "espnow_iperf_initiator", 4 * 1024,
        //         NULL, tskIDLE_PRIORITY + 1, NULL);

}
