#ifndef GLOBAL_NVS_H
#define GLOBAL_NVS_H

#include "esp_mac.h"
#include "esp_log.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

void get_nvs_info(void);

void erase_nvs(void);

void nvs_init(void);

void set_nvs_info(const char *tag, const char *value);

void set_reset_count(uint8_t reset_count);

void get_nvs_info_uint8_t_array(const char *value_str, uint8_t* value);

void set_nvs_info_uint8_t_array(const char *tag, uint8_t* value, int length);

void set_nvs_info_set_slave_mac(uint8_t slave_num, uint8_t* value);

void set_nvs_info_set_host_message(uint8_t host_mac[6], uint8_t host_channel, char username[100]);

void reset_elabel();

// Language NVS functions
void get_language_nvs_info(char *language);
void set_language_nvs_info(const char *language);
void init_language_nvs(void);

#ifdef __cplusplus
}
#endif

#endif