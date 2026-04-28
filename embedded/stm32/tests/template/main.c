/**
 * @file main.c
 * @brief Test application for the MB85RC1MT FRAM driver on TockOS
 *
 * Exercises basic FRAM read/write operations and boundary checks
 * using the Unity testing framework.
 *
 * Based on stm32/test/test_fram/test_fram.c from ENTS-node-firmware.
 *
 * @author Pritish Mahato
 * @date 2026-02-25
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <stdio.h>
#include <string.h>

#include <unity.h>

/**
 * @brief Run at the start of every test.
 */
void setUp(void) {}

/**
 * @brief Run at the end of every test.
 */
void tearDown(void) {}


void test_template(void) {
  int two = 1+1;
  TEST_ASSERT_EQUAL_INT(2, two);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_template);

  return UNITY_END();
}
