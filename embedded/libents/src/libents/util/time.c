#include "time.h"

#include <time.h>

#include <libtock/peripherals/rtc.h>

uint32_t epoch(void) {
    libtock_rtc_date_t date = {};
    libtocksync_rtc_get_date(&date);
    return rtc_date_to_epoch(&date);
}

uint32_t rtc_date_to_epoch(const libtock_rtc_date_t* date) {
    struct tm tm_time = {0};

    tm_time.tm_year = date->year - 1900; // years since 1900
    tm_time.tm_mon  = date->month - 1;   // months since January [0-11]
    tm_time.tm_mday = date->day;
    tm_time.tm_hour = date->hour;
    tm_time.tm_min  = date->minute;
    tm_time.tm_sec  = date->seconds;

    return (uint32_t) mktime(&tm_time);
}
