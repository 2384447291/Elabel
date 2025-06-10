#ifndef GLOBAL_TIME_H
#define GLOBAL_TIME_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "esp_timer.h"
#include <sys/time.h>

//--------------------------------------SNTP时间同步函数-----------------------------------------//
#ifdef __cplusplus
extern "C" {
#endif
// void SNTP_syset_time(void);

void HTTP_syset_time(void);

void EspNow_syset_time(long long nowTime);

long long get_unix_time(void);

void get_unix_time_str(char* str_time, size_t size);

void get_clock_time(char* str_time);

void Log_time(void);

void get_open_time(char* str_time);
#ifdef __cplusplus
}
#endif
//--------------------------------------SNTP时间同步函数-----------------------------------------//

//挂墙时钟
extern uint32_t elabelUpdateTick;
#endif