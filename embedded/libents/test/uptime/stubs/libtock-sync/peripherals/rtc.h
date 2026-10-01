/**
 * @file rtc.h
 * @brief Host stub for the Tock RTC types.
 *
 * libents/util/time.h includes this for libtock_rtc_date_t. The uptime library
 * only ever calls epoch(), so the struct just has to exist and have the right
 * shape.
 */

#pragma once

#include <stdint.h>

typedef struct {
  uint32_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t seconds;
} libtock_rtc_date_t;
