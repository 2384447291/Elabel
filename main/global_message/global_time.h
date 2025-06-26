#ifndef GLOBAL_TIME_H
#define GLOBAL_TIME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "esp_timer.h"
#include <sys/time.h>
typedef struct 
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}time_description;

static const char* month_abbr[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* week_abbr[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* data_abbr[] = {
    "1st", "2nd", "3rd", "4th", "5th", "6th",
    "7th", "8th", "9th", "10th", "11th", "12th",
    "13th", "14th", "15th", "16th", "17th", "18th",
    "19th", "20th", "21st", "22nd", "23rd", "24th",
    "25th", "26th", "27th", "28th", "29th", "30th",
    "31st"
};

static const char* time_abbr[] = {
    "12 AM", "1 AM", "2 AM", "3 AM", "4 AM", "5 AM",
    "6 AM", "7 AM", "8 AM", "9 AM", "10 AM", "11 AM",
    "12 PM", "1 PM", "2 PM", "3 PM", "4 PM", "5 PM",
    "6 PM", "7 PM", "8 PM", "9 PM", "10 PM", "11 PM"
};


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

time_description get_date_time(void);
#ifdef __cplusplus
}
#endif
//--------------------------------------SNTP时间同步函数-----------------------------------------//

//挂墙时钟
extern uint32_t elabelUpdateTick;
#endif