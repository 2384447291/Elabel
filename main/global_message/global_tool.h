#ifndef GLOBAL_TOOL_H
#define GLOBAL_TOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"


#ifdef __cplusplus
extern "C" {
#endif

void progress_update(size_t sent_before, size_t sent_now, size_t total);

#ifdef __cplusplus
}
#endif


#endif