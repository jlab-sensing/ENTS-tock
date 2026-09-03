#include "user_config.h"

#include <libents/controller/modules/power.h>
#include <libents/controller/modules/wifi.h>
#include <libents/controller/modules/wifi_userconfig.h>
#include <libents/user_config.h>
#include <libtock/net/eui64.h>
#include <libtock/services/alarm.h>
#include <ulog.h>

UserConfigState state = USERCONFIG_STATE_OFF;

static libtock_alarm_t alarm = {};

/**
 * @brief Stops the user config webserver
 *
 * @see libtock_alarm_callback
 */
void UserConfigStopCallback(uint32_t, uint32_t, void* ptr);

void UserConfigStart(uint32_t retry_ms) {
  // variables to store WiFi host info
  char ssid[255] = {};
  char ip[16] = {};
  char mac[18] = {};

  // constant password for AP
  const char pass[] = "ilovedirt";

  // Reload user config from FRAM
  UserConfigStatus status_load = UserConfigLoad();

  // start user config interface
  if (status_load == USERCONFIG_OK) {
    // print current user config
    ulog_info("Current user configuration:");
    ulog_info("---------------------------");
    UserConfigPrint();
  } else {
    ulog_error("Could not load user config.");
  }

  // Get device address
  uint64_t dev_eui = 0;
  libtock_eui64_get(&dev_eui);
  uint32_t devAddr = (uint32_t)(dev_eui & 0xFFFFFFFF);

  snprintf(ssid, sizeof(ssid), "ents-%08lX", devAddr);

  bool controller_status = true;
  controller_status = ControllerWiFiHost(ssid, pass);
  if (controller_status) {
    ulog_info("Successfully started WiFi AP!");
  } else {
    ulog_error("Failed to start WiFi AP!");
    return;
  }

  controller_status = ControllerUserConfigStart();
  if (controller_status) {
    ulog_info("Successfully started user config webserver!");
  } else {
    ulog_error("Failed to start user config webserver!");
    return;
  }

  ulog_info("Sending FRAM configuration to ESP32...");
  UserConfigStatus status = ControllerUserConfigSend();
  if (status != USERCONFIG_OK) {
    ulog_info("Failed to send config to ESP32: %d", status);
  }

  // Get host info
  ControllerWiFiHostInfo(ssid, ip, mac, NULL);
  ulog_info("WiFi AP Info:");
  ulog_info("---------------");
  ulog_info("ssid \"%s\"", ssid);
  ulog_info("pass \"%s\"", pass);
  ulog_info("User Config http://%s/", ip);
  ulog_info("WiFi AP MAC: \"%s\"", mac);

  state = USERCONFIG_STATE_ON;

  // autostop if retry_ms is specified
  if (retry_ms > 0) {
    UserConfigStop(retry_ms);
  }
}

void UserConfigUpdateFromServer(void) {
  // Reload user config from FRAM
  // UserConfigStatus status_load = UserConfigLoad();

  // Get Config from esp32
  ulog_info("Requesting configuration from ESP32...");
  UserConfigStatus status = ControllerUserConfigRequest();

  if (status != USERCONFIG_OK) {
    ulog_error("Something went wrong with getting user config");
  }

  // Reload user config from FRAM
  if (UserConfigLoad() != USERCONFIG_OK) {
    ulog_error("Error saved configuration not valid!");
    ulog_info("Try sending configuration again.");

    while (1);
  }

  // Print updated config
  ulog_info("Updated user configuration:");
  ulog_info("---------------------------");
  UserConfigPrint();
}

void UserConfigStop(uint32_t retry_ms) {
  state = USERCONFIG_STATE_STOPPING;
  libtock_alarm_in_ms(retry_ms, UserConfigStopCallback, (void*)&retry_ms,
                      &alarm);
}

void UserConfigStopCallback(uint32_t, uint32_t, void* ptr) {
  ulog_info("Stopping UserConfig webserver...\t");

  // deference to retry delay
  uint32_t retry_ms = *(uint32_t*)ptr;

  uint8_t clients = 0;
  ControllerWiFiHostInfo(NULL, NULL, NULL, &clients);

  // Handle if there are still clients connected
  if (clients > 0) {
    ulog_error("Error! %d clients still connected!", clients);
    UserConfigStop(retry_ms);
    return;
  }

  // Try to stop WiFI
  if (!ControllerWiFiStopHost()) {
    ulog_info("Error! Could not stop WiFi network!");
    UserConfigStop(retry_ms);
  } else {
    state = USERCONFIG_STATE_OFF;
    ulog_info("Stopped!");

    // if uploading via LoRaWAN deep sleep esp32
    const UserConfiguration* cfg = UserConfigGet();
    if (cfg->Upload_method == Uploadmethod_LoRa) {
      ulog_info("Putting esp in deep sleep mode...");
      ControllerPowerSleep();
    }
  }
}

int UserConfigCurrentStatus(void) { return state; }
