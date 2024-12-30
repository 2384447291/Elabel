#ifndef GLOBAL_TIME_H
#define GLOBAL_TIME_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//--------------------------------------SNTP时间同步函数-----------------------------------------//
#ifdef __cplusplus
extern "C" {
#endif
// void SNTP_syset_time(void);

void HTTP_syset_time(void);

long long get_unix_time(void);
#ifdef __cplusplus
}
#endif
//--------------------------------------SNTP时间同步函数-----------------------------------------//

//挂墙时钟
extern uint32_t elabelUpdateTick;
#endif