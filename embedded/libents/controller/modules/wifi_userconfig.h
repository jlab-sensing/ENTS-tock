/**
 * @file wifi_userconfig.h
 * @date 2026-04-08
 * @author Ahmed Falah
 * @author John Madden <jmadden173@pm.me>
 * @brief User configuration WiFi interface
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../communication.h"

// TODO Remove dependency
#include "userConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup controller
 * @defgroup wifiUserConfig WiFi User Config
 * @brief User configuration WiFi interface
 *
 * TODO
 */

/**
 * @brief Request user configuration from ESP32
 *
 * @return UserConfigStatus indicating success or failure
 */
UserConfigStatus ControllerUserConfigRequest(void);

/**
 * @brief Send user configuration to ESP32
 *
 * @return UserConfigStatus indicating success or failure
 */
UserConfigStatus ControllerUserConfigSend(void);

/**
 * @brief Start the user configuration website.
 *
 * This function must be called after the station has been setup.
 *
 * @return true if command was sent sucessfully, false otherwise
 */
bool ControllerUserConfigStart(void);

/**
 * @brief Check if configuration is empty/uninitialized
 *
 * @param config Pointer to UserConfiguration structure
 * @return true if config is empty, false otherwise
 */
bool isConfigEmpty(const UserConfiguration *config);

/**
 * @brief Print user configuration details to log
 *
 * @param config Pointer to UserConfiguration structure
 */
void printUserConfig(const UserConfiguration *config);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
