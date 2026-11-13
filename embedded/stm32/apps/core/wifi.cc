/**
 * @file wifi.cc
 *
 * @author John Madden (jmadden173@pm.me)
 *
 * Ported from the STM32Cube firmware's wifi.c to the libents controller API.
 */

#include "wifi.h"

#include <libents/controller/modules/wifi.h>
#include <libents/user_config.h>
#include <libents/util/time.h>
#include <libtock-sync/services/alarm.h>
#include <ulog.h>

/** Maximum number of status checks for each command */
static const unsigned int max_retries = 10;

/** Delay between status checks */
static const int retry_delay_ms = 1000;

/**
 * @brief Waits for the response to the last HTTP request
 *
 * @return HTTP code, 0 if the request was not sent or timed out
 */
static unsigned int wifi_wait_for_request(void);

static int wifi_connect(void) {
  ulog_trace("wifi_connect");

  const UserConfiguration* cfg = UserConfigGet();

  ulog_info("Connecting to %s", cfg->WiFi_SSID);
  if (!ControllerWiFiConnect(cfg->WiFi_SSID, cfg->WiFi_Password)) {
    ulog_error("Could not communicate with esp32");
    return -1;
  }

  for (unsigned int retries = 0; retries < max_retries; retries++) {
    libtocksync_alarm_delay_ms(retry_delay_ms);

    ControllerWiFiStatus status = ControllerWiFiCheckWiFi();
    ulog_debug("WiFi status: %d", status);

    if (status == CONTROLLER_WIFI_CONNECTED) {
      ulog_info("Connected to %s", cfg->WiFi_SSID);
      return 0;
    } else if (status == CONTROLLER_WIFI_NO_SSID_AVAIL) {
      ulog_error("SSID %s not available", cfg->WiFi_SSID);
      return -1;
    } else if (status == CONTROLLER_WIFI_CONNECT_FAILED) {
      ulog_error("Connection to %s failed", cfg->WiFi_SSID);
      return -1;
    }
  }

  ulog_error("Timeout connecting to %s", cfg->WiFi_SSID);
  return -1;
}

static int wifi_check_api(void) {
  ulog_trace("wifi_check_api");

  const UserConfiguration* cfg = UserConfigGet();

  ulog_info("Checking API health of %s", cfg->API_Endpoint_URL);
  if (!ControllerWiFiCheckApi(cfg->API_Endpoint_URL)) {
    ulog_error("Could not communicate with esp32");
    return -1;
  }

  unsigned int http_code = wifi_wait_for_request();
  if (http_code != 200) {
    ulog_error("API check failed (HTTP code: %d)", http_code);
    return -1;
  }

  ulog_info("API is reachable");
  return 0;
}

static unsigned int wifi_wait_for_request(void) {
  ControllerWiFiResponse resp = {};

  for (unsigned int retries = 0; retries < max_retries; retries++) {
    libtocksync_alarm_delay_ms(retry_delay_ms);

    resp = ControllerWiFiCheckRequest();
    ulog_debug("HTTP code: %d", resp.http_code);

    if (resp.http_code != 0) {
      break;
    }
  }

  if (resp.http_code == 0) {
    ulog_error("No HTTP response, likely an error with WiFi or a timeout");
  }

  return resp.http_code;
}

int wifi_init(void) {
  ulog_trace("wifi_init");

  int ret = wifi_connect();
  if (ret < 0) {
    ControllerWiFiDisconnect();
    return ret;
  }

  return wifi_check_api();
}

int wifi_timesync(void) {
  ulog_trace("wifi_timesync");

  if (!ControllerWiFiNtpSync()) {
    ulog_error("Could not communicate with esp32");
    return -1;
  }

  for (unsigned int retries = 0; retries < max_retries; retries++) {
    libtocksync_alarm_delay_ms(retry_delay_ms);

    uint32_t unix_epoch = ControllerWiFiTime();
    if (unix_epoch != 0) {
      ulog_info("Current unix epoch is %lu", unix_epoch);
      set_epoch(unix_epoch);
      return 0;
    }
  }

  ulog_error("Timeout waiting for NTP time");
  return -1;
}

int wifi_upload(const uint8_t* buffer, int length) {
  ulog_trace("wifi_upload");

  if (!ControllerWiFiPost(buffer, (size_t)length)) {
    ulog_error("Could not communicate with esp32");
    return -1;
  }

  unsigned int http_code = wifi_wait_for_request();
  if (http_code != 200) {
    ulog_error("Upload failed (HTTP code: %d)", http_code);
    return -1;
  }

  return 0;
}
