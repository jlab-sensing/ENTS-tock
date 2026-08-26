/**
 * @file main.c
 * @brief Test the RTC peripherial is functional
 *
 * Checks the following:
 * - RTC driver is enabled in the kernel
 * - Date can be set
 * - Date can be get
 * - Date is retained after going to sleep
 *
 * @author John Madden
 * @date 2027-07-23
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <libents/util/time.h>
#include <libtock-sync/peripherals/rtc.h>
#include <libtock-sync/services/alarm.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

static const int delay = 10;
static const int delay_ms = delay * 1000;

/**
 * @brief Run at the start of every test.
 */
void setUp(void) {}

/**
 * @brief Run at the end of every test.
 */
void tearDown(void) {}

void test_rtc_exists(void) {
  bool exists = libtock_rtc_exists();
  TEST_ASSERT(exists);
}

void test_set_date(void) {
  int ret = 0;

  // Initialises a date struct with a certain timestamp
  libtock_rtc_date_t date = {.year = 2023,
                             .month = JANUARY,
                             .day = 1,

                             .day_of_week = MONDAY,
                             .hour = 12,
                             .minute = 30,
                             .seconds = 1};

  ret = libtocksync_rtc_set_date(&date);

  TEST_ASSERT_EQUAL(0, ret);
}

void test_get_date(void) {
  int ret = 0;
  libtock_rtc_date_t date = {};

  ret = libtocksync_rtc_get_date(&date);

  TEST_ASSERT_EQUAL(0, ret);

  TEST_ASSERT_EQUAL(2023, date.year);
  TEST_ASSERT_EQUAL(JANUARY, date.month);
  TEST_ASSERT_EQUAL(1, date.day);
  TEST_ASSERT_EQUAL(MONDAY, date.day_of_week);
  TEST_ASSERT_EQUAL(12, date.hour);
  TEST_ASSERT_EQUAL(30, date.minute);
  TEST_ASSERT_INT_WITHIN(2, 1, date.seconds);
}

void test_get_date_delay(void) {
  int ret = 0;
  libtock_rtc_date_t date = {};

  libtocksync_alarm_delay_ms(delay_ms);

  ret = libtocksync_rtc_get_date(&date);

  TEST_ASSERT_EQUAL(0, ret);

  TEST_ASSERT_EQUAL(2023, date.year);
  TEST_ASSERT_EQUAL(JANUARY, date.month);
  TEST_ASSERT_EQUAL(1, date.day);
  TEST_ASSERT_EQUAL(MONDAY, date.day_of_week);
  TEST_ASSERT_EQUAL(12, date.hour);
  TEST_ASSERT_EQUAL(30 + (delay / 60), date.minute);
  TEST_ASSERT_INT_WITHIN(2, (1 + delay) % 60, date.seconds);
}

void test_time_epoch(void) {
  uint32_t epoch_set = 1785878164;
  set_epoch(epoch_set);
  uint32_t epoch_get = epoch();

  TEST_ASSERT_INT_WITHIN(2, epoch_set, epoch_get);
}

void test_rtc_date_to_epoch(void) {
  libtock_rtc_date_t date = {.year = 2026,
                             .month = AUGUST,
                             .day = 4,

                             .day_of_week = TUESDAY,
                             .hour = 21,
                             .minute = 7,
                             .seconds = 49};

  uint32_t epoch = rtc_date_to_epoch(&date);

  TEST_ASSERT_EQUAL(1785877669, epoch);
}

void test_epoch_to_rtc_date(void) {
  libtock_rtc_date_t date = {};

  uint32_t epoch = 1785877669;

  epoch_to_rtc_date(epoch, &date);

  TEST_ASSERT_EQUAL(2026, date.year);
  TEST_ASSERT_EQUAL(AUGUST, date.month);
  TEST_ASSERT_EQUAL(4, date.day);
  // TEST_ASSERT_EQUAL(TUESDAY, date.day_of_week);
  TEST_ASSERT_EQUAL(21, date.hour);
  TEST_ASSERT_EQUAL(7, date.minute);
  TEST_ASSERT_EQUAL(49, date.seconds);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_rtc_exists);
  RUN_TEST(test_set_date);
  RUN_TEST(test_get_date);
  RUN_TEST(test_get_date_delay);
  RUN_TEST(test_rtc_date_to_epoch);
  RUN_TEST(test_epoch_to_rtc_date);
  RUN_TEST(test_time_epoch);

  return UNITY_END();
}
