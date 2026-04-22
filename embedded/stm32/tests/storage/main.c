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

#include <stdio.h>
#include <string.h>

#include <unity.h>

#include <storage/fram.h>

// ---------------------------------------------------------------------------
// Forward declarations for test functions.
// Required by -Wmissing-prototypes and -Wmissing-declarations.
// ---------------------------------------------------------------------------

void test_fram_size(void);
void test_write_valid_data(void);
void test_write_out_of_range(void);
void test_write_zero_length(void);
void test_read_valid_data(void);
void test_read_out_of_range(void);
void test_read_zero_length(void);
void test_read_status(void);
void test_write_read_multiple_addresses(void);

// ---------------------------------------------------------------------------
// Unity required callbacks (declared in unity.h, defined here).
// ---------------------------------------------------------------------------

void setUp(void) {
}

void tearDown(void) {
}

// ---------------------------------------------------------------------------
// Test Cases (ported from stm32/test/test_fram/test_fram.c)
// ---------------------------------------------------------------------------

void test_fram_size(void) {
  TEST_ASSERT_EQUAL_UINT32(131072, fram_size());
}

void test_write_valid_data(void) {
  uint8_t data[]    = {1, 2, 3, 4, 5};
  fram_status status = fram_write(0x00, data, sizeof(data));
  TEST_ASSERT_EQUAL_INT(FRAM_OK, status);
}

void test_write_out_of_range(void) {
  uint8_t data[]    = {1, 2, 3, 4, 5};
  fram_addr addr     = fram_size() + 1;
  fram_status status = fram_write(addr, data, sizeof(data));
  TEST_ASSERT_EQUAL_INT(FRAM_OUT_OF_RANGE, status);
}

void test_write_zero_length(void) {
  uint8_t data[]    = {1, 2, 3, 4, 5};
  fram_status status = fram_write(0x00, data, 0);
  TEST_ASSERT_EQUAL_INT(FRAM_OK, status);
}

void test_read_valid_data(void) {
  uint8_t write_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
  fram_addr addr        = 0x100;

  fram_status status = fram_write(addr, write_data, sizeof(write_data));
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "write failed");

  uint8_t read_data[5] = {0};
  status = fram_read(addr, sizeof(read_data), read_data);
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "read failed");

  TEST_ASSERT_EQUAL_UINT8_ARRAY(write_data, read_data, sizeof(write_data));
}

void test_read_out_of_range(void) {
  uint8_t data[5];
  fram_addr addr     = fram_size() + 1;
  fram_status status = fram_read(addr, sizeof(data), data);
  TEST_ASSERT_EQUAL_INT(FRAM_OUT_OF_RANGE, status);
}

void test_read_zero_length(void) {
  uint8_t data[5];
  fram_status status = fram_read(0x00, 0, data);
  TEST_ASSERT_EQUAL_INT(FRAM_OK, status);
}

void test_read_status(void) {
  uint8_t data[5];
  fram_status status = fram_read(0x00, sizeof(data), data);
  TEST_ASSERT_EQUAL_INT(FRAM_OK, status);
}

void test_write_read_multiple_addresses(void) {
  uint8_t pattern_a[] = {0xAA, 0xBB, 0xCC};
  uint8_t pattern_b[] = {0x11, 0x22, 0x33};
  fram_addr addr_a     = 0x200;
  fram_addr addr_b     = 0x300;

  fram_status status;

  status = fram_write(addr_a, pattern_a, sizeof(pattern_a));
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "write A failed");

  status = fram_write(addr_b, pattern_b, sizeof(pattern_b));
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "write B failed");

  uint8_t read_a[3] = {0};
  uint8_t read_b[3] = {0};

  status = fram_read(addr_a, sizeof(read_a), read_a);
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "read A failed");

  status = fram_read(addr_b, sizeof(read_b), read_b);
  TEST_ASSERT_EQUAL_INT_MESSAGE(FRAM_OK, status, "read B failed");

  TEST_ASSERT_EQUAL_UINT8_ARRAY(pattern_a, read_a, 3);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(pattern_b, read_b, 3);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_fram_size);
  RUN_TEST(test_write_valid_data);
  RUN_TEST(test_write_zero_length);
  RUN_TEST(test_write_out_of_range);
  RUN_TEST(test_read_status);
  RUN_TEST(test_read_zero_length);
  RUN_TEST(test_read_out_of_range);
  RUN_TEST(test_read_valid_data);
  RUN_TEST(test_write_read_multiple_addresses);

  return UNITY_END();
}
