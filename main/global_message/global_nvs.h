#ifndef __GLOBAL_NVS_H__
#define __GLOBAL_NVS_H__

#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

void get_nvs_info(void);

void erase_nvs(void);

void nvs_init(void);

void set_nvs_info(const char *tag, const char *value);

void set_nvs_info_uint8_t(const char *tag, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif