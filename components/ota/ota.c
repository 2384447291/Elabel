#include "ota.h"
#include "global_message.h"
#include "global_tool.h"
#define TAG "OTA"   
#define OTA_BUFFER_SIZE 4 * 1024

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}

ota_state m_ota_state = no_need_ota;

ota_state get_ota_status(void)
{
    return m_ota_state;
}

void set_ota_status(ota_state _ota_state)
{
    m_ota_state = _ota_state;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    return err;
}

void print_app_desc(const esp_app_desc_t* d)
{
    ESP_LOGI(TAG, "magic_word:        0x%"PRIx32, d->magic_word);
    ESP_LOGI(TAG, "secure_version:    %"PRIu32,  d->secure_version);
    ESP_LOGI(TAG, "version:           %s",      d->version);
    ESP_LOGI(TAG, "project_name:      %s",      d->project_name);
    ESP_LOGI(TAG, "compile time:      %s %s",   d->date, d->time);
    ESP_LOGI(TAG, "idf_ver:           %s",      d->idf_ver);
    // 打印 SHA256
    {
        char buf[65] = {0};
        for (int i = 0; i < 32; i++) {
            sprintf(buf + i*2, "%02x", d->app_elf_sha256[i]);
        }
        ESP_LOGI(TAG, "app_elf_sha256:    %s", buf);
    }
    // eFuse block rev（major.minor = rev_full/100 . rev_full%100）
    uint16_t min = d->min_efuse_blk_rev_full;
    uint16_t max = d->max_efuse_blk_rev_full;
    ESP_LOGI(TAG, "min_efuse_rev:     v%u.%02u", min/100, min%100);
    ESP_LOGI(TAG, "max_efuse_rev:     v%u.%02u", max/100, max%100);
    ESP_LOGI(TAG, "mmu_page_size log: %u (=> %u bytes)", d->mmu_page_size,
             (1U << d->mmu_page_size));
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    print_app_desc(new_app_info);

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }
    return ESP_OK;
}

int image_size;
int current_size;

void simple_ota_example_task(void *pvParameter)
{
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    esp_err_t ota_finish_err = ESP_OK;
    
    esp_http_client_config_t http_config = {
        .url = get_global_data()->m_newest_firmware_url,
        .keep_alive_enable = true,
        .buffer_size   = OTA_BUFFER_SIZE,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .http_client_init_cb = _http_client_init_cb, 
        .bulk_flash_erase = true,
        .partial_http_download = false,
        .buffer_caps = MALLOC_CAP_INTERNAL,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        vTaskDelete(NULL);
    }

    image_size = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "Starting OTA from %s, size is: %d", http_config.url, image_size);

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        current_size = esp_https_ota_get_image_len_read(https_ota_handle);
        progress_update(current_size - OTA_BUFFER_SIZE, current_size, image_size);
    }
    
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) 
    {
        ESP_LOGE(TAG, "Complete data was not received.");
        goto ota_end;
    } 
    else 
    {
        //从这里进去esp_https_ota_finish-->esp_ota_end-->esp_image_verify-->process_segments-->process_segment-->process_segment_data-->bootloader_common_check_efuse_blk_validity
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) 
        {
            ESP_LOGI(TAG, "OTA success");
            set_ota_status(ota_success);
            vTaskDelete(NULL);
        } 
        else 
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) 
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            goto ota_end;
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    set_ota_status(ota_fail);
    vTaskDelete(NULL);
}

void start_ota(void)
{
    m_ota_state = ota_ing;
    xTaskCreate(&simple_ota_example_task, "ota_task", 8192, NULL, 10, NULL);
}

float get_ota_progress(void)
{
    return current_size * 100 / image_size;
}
