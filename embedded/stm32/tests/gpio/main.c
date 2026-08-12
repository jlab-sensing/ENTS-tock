/**
 * @file main.c
 * @brief Test gpio functionality
 *
 * Checks that the gpio kernel syscall is enabled and outputs a 10 Hz signal on
 * the PG pin.
 *
 * @author John Madden
 * @date 2026-07-03
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <libtock-sync/services/alarm.h>
#include <libtock/peripherals/gpio.h>
#include <unity.h>

#ifndef OUTPUT_PIN
#define OUTPUT_PIN 3
#endif  // OUTPUT_PIN

static const int output_pin = OUTPUT_PIN;

/**
 * @brief Run at the start of every test.
 */
void setUp(void) {}

/**
 * @brief Run at the end of every test.
 */
void tearDown(void) {}

void test_driver(void) {
  bool exists = libtock_gpio_exists();

  TEST_ASSERT_TRUE(exists);
}

void test_output(void) {
  libtock_gpio_enable_output(output_pin);

  for (int i = 0; i < 10; i++) {
    libtock_gpio_toggle(output_pin);
    libtocksync_alarm_delay_ms(100);
  }
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_driver);
  RUN_TEST(test_output);

  return UNITY_END();
}
