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

#include <libents/controller/controller.h>
#include <libents/controller/modules/wifi.h>
#include <libents/controller/modules/wifi_userconfig.h>
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

void test_ControllerWiFiHost(void) {
  int ret = 0;

  ret = ControllerWiFiHost("ents-test", "ilovedirt");
  TEST_ASSERT_EQUAL(1, ret);
}

void test_ControllerWiFiHostInfo(void) {
  char ssid[32] = {};
  char ip[32] = {};
  char mac[32] = {};
  uint8_t clients = 0;

  ControllerWiFiHostInfo(ssid, ip, mac, &clients);

  TEST_ASSERT_EQUAL_STRING("ents-test", ssid);
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", ip);
  TEST_ASSERT_EQUAL(0, clients);
}

void test_ControllerUserConfigStart(void) {
  int ret = 0;

  ret = ControllerUserConfigStart();
  TEST_ASSERT_EQUAL(1, ret);
}

void test_ControllerWiFiStopHost(void) {
  int ret = 0;

  ret = ControllerWiFiStopHost();
  TEST_ASSERT_EQUAL(1, ret);
}

int main(void) {
  ControllerInit();

  UNITY_BEGIN();

  RUN_TEST(test_ControllerWiFiHost);
  RUN_TEST(test_ControllerWiFiHostInfo);
  RUN_TEST(test_ControllerWiFiStopHost);

  return UNITY_END();
}
