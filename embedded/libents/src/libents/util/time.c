#include "time.h"

#include <libtock/peripherals/rtc.h>

// Days in each month (non-leap year)
static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};

// Check if year is a leap year
static inline int is_leap_year(uint16_t year) {
  return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

// Get number of days in February for a given year
static inline uint8_t get_feb_days(uint16_t year) {
  return is_leap_year(year) ? 29 : 28;
}

uint32_t epoch(void) {
  libtock_rtc_date_t date = {};
  libtocksync_rtc_get_date(&date);
  return rtc_date_to_epoch(&date);
}

void set_epoch(uint32_t epoch) {
  libtock_rtc_date_t date = {};
  epoch_to_rtc_date(epoch, &date);
  libtocksync_rtc_set_date(&date);
}

uint32_t rtc_date_to_epoch(const libtock_rtc_date_t* date) {
  // Unix epoch is January 1, 1970
  const uint16_t epoch_year = 1970;
  uint32_t total_seconds = 0;
  uint16_t year;

  // Count days from 1970 to the start of the current year
  for (year = epoch_year; year < date->year; year++) {
    total_seconds += is_leap_year(year) ? 366 : 365;
  }
  total_seconds *= 86400;  // Convert days to seconds

  // Count days from start of year to start of current month
  uint32_t days = 0;
  for (uint8_t month = 0; month < (date->month - 1); month++) {
    days += days_in_month[month];
    // Add leap day if February of a leap year
    if (month == 1 && is_leap_year(date->year)) {
      days++;
    }
  }
  total_seconds += days * 86400;

  // Add days of current month (day is 1-indexed, subtract 1)
  total_seconds += (date->day - 1) * 86400;

  // Add hours, minutes, seconds
  total_seconds += (uint32_t)date->hour * 3600;
  total_seconds += (uint32_t)date->minute * 60;
  total_seconds += date->seconds;

  return total_seconds;
}

void epoch_to_rtc_date(uint32_t epoch, libtock_rtc_date_t* date) {
  const uint16_t epoch_year = 1970;

  uint32_t days = epoch / 86400;
  uint32_t seconds = epoch % 86400;

  // Determine year
  uint16_t year = epoch_year;
  while (true) {
    uint16_t days_in_year = is_leap_year(year) ? 366 : 365;
    if (days < days_in_year) {
      break;
    }
    days -= days_in_year;
    year++;
  }
  date->year = year;

  // Determine month
  uint8_t month = 0;
  while (true) {
    uint8_t month_days = days_in_month[month];
    if (month == 1 && is_leap_year(year)) {
      month_days++;
    }

    if (days < month_days) {
      break;
    }

    days -= month_days;
    month++;
  }
  date->month = month + 1;  // Convert to 1-indexed month

  // Day of month (1-indexed)
  date->day = days + 1;

  // Time of day
  date->hour = seconds / 3600;
  seconds %= 3600;

  date->minute = seconds / 60;
  date->seconds = seconds % 60;
}
