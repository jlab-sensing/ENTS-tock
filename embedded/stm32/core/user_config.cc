#include "user_config.h"

#include <libtock/services/alarm.h>



#include <ulog.h>


#include <libents/user_config.h>
#include <libents/controller/modules/wifi.h>
#include <libents/controller/modules/wifi_userconfig.h>
#include <libents/controller/modules/power.h>



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
  
  uint32_t devAddr = 0;
  //GetDevAddr(&devAddr);
  snprintf(ssid, sizeof(ssid), "ents-%08X", devAddr);
 
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

  // Get host info
  ControllerWiFiHostInfo(ssid, ip, mac, NULL);
  ulog_info("WiFi AP Info:");
  ulog_info("---------------");
  ulog_info("ssid \"%s\"", ssid);
  ulog_info("pass \"%s\"", pass);
  ulog_info("User Config http://%s/", ip);
  ulog_info("WiFi AP MAC: \"%s\"", mac);
  
  // Get Config from esp32
  ulog_info("Requesting configuration from ESP32...");
  UserConfigStatus status = ControllerUserConfigRequest();

  // If esp32 responded with an empty config
  if (status == USERCONFIG_EMPTY_CONFIG || status != USERCONFIG_OK) {
    // Don't do anything if we don't have a saved config
    if (status_load != USERCONFIG_OK) {
      ulog_info("No configuration to send to ESP32!");
      // it's a trap! No valid userconfig
      // Waiting for new configuration on reset
      while (1);

    // Otherwise send the saved config and continue
    } else {
      // If ESP32 has empty config or request failed, send our config
      ulog_info("Sending FRAM configuration to ESP32...");
      status = ControllerUserConfigSend();

      if (status != USERCONFIG_OK) {
        ulog_info("Failed to send config to ESP32: %d",
            status);
      }
    }

  // if ESP32 provided a config
  } else { 
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

  state = USERCONFIG_STATE_ON;

  // autostop if retry_ms is specified
  if (retry_ms > 0) {
    UserConfigStop(retry_ms);
  }
}


void UserConfigStop(uint32_t retry_ms) {
  state = USERCONFIG_STATE_STOPPING;
  libtock_alarm_in_ms(retry_ms, UserConfigStopCallback, (void*) &retry_ms, &alarm);
}


void UserConfigStopCallback(uint32_t, uint32_t, void* ptr) {
  ulog_info("Stopping UserConfig webserver...\t");

  // deference to retry delay
  uint32_t retry_ms = *(uint32_t*) ptr; 

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

int UserConfigCurrentStatus(void) {
  return state;
}
