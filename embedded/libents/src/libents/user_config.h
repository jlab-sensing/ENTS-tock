/**
 * @file user_config.h.c
 * @author Ahmed Hassan Falah
 * @brief User configuration library.
 *
 * Handles the management of user configuration. This mainly applies to
 * enabling various sensors, setting calibration values, and configuring WiFi.
 *
 * Ported on 2026-05-20 by John Madden
 *
 * @date    10/13/2024
 * copyright [2024] <Ahmed Hassan Falah>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./proto/user_config.pb.h"

/**
 * @ingroup stm32
 * @defgroup userConfig User Configuration
 * @brief   Soil Power Sensor userConfig library
 *
 * This library provies functions for saving and loading user configurations
 * from onboard non-volatile memory in the form of FRAM. The user configuration
 * is assigned a block of memory where the encoded data is stored. The memory
 * is structure in the form of [length, encoded data].
 *
 * There is an optional #define TEST_USER_CONFIG that is used to guarentee
 * a certain configuration is returned by functions for testing purposes.
 *
 * @{
 */

// Maximum size of the received data buffer (from protobuf definition).
#define RX_BUFFER_SIZE UserConfiguration_size
// Starting address for user config data in FRAM.
#define USER_CONFIG_START_ADDRESS 1794
// Address for storing the user config data length in FRAM.
#define USER_CONFIG_LEN_ADDR 1792

typedef enum {
  USERCONFIG_OK = 0,
  USERCONFIG_FRAM_ERROR = -1,
  USERCONFIG_DECODE_ERROR = -2,
  USERCONFIG_NO_RESPONSE = -3,
  USERCONFIG_COMM_ERROR = -4,
  USERCONFIG_INVALID_RESPONSE = -5,
  USERCONFIG_NOT_LOADED = -6,
  USERCONFIG_EMPTY_CONFIG = -7,
  USERCONFIG_ENCODE_ERROR = -8,
  USERCONFIG_NULL_CONFIG = -9,
} UserConfigStatus;

/**
 * @brief Loads user configuration data from FRAM to RAM.
 *
 * This function reads the stored user configuration data from FRAM,
 * decodes it, and loads it into RAM. The data will be stored in a
 * static UserConfig structure.
 *
 * @return UserConfigStatus - USERCONFIG_OK if successful, error code
 * otherwise.
 */
UserConfigStatus UserConfigLoad(void);

/**
 * @brief Saves the configuration to FRAM.
 *
 * @param config  Pointer to the UserConfiguration structure to save.
 * @return USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfigSave(const UserConfiguration* config);

/**
 * @brief Gets a reference to the loaded user configuration data in RAM.
 *
 * This function returns a pointer to the UserConfig structure in RAM, allowing
 * access to the loaded configuration without reading from FRAM.
 *
 * @return Pointer to the loaded UserConfig structure.
 */
const UserConfiguration* UserConfigGet(void);

/**
 * @brief Prints arbitrary user config over serial.
 *
 * The printing/formatting is done with APP_LOG functions requiring
 * SystemApp_Init() to be called before this function can be used.
 */
void UserConfigPrintAny(const UserConfiguration* config);

/**
 * @brief Prints the current user configuration over serial.
 *
 * @see UserConfigPrintAny
 */
void UserConfigPrint(void);

/**
 * @brief Clears the user configuration stored in FRAM.
 *
 * This function sets the userconfig to all zeros in FRAM.
 *
 * @return USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfigClear(void);

/**
 * @brief Gets encoded bytes from userconfig
 *
 * @param buffer Output buffer.
 * @param length Length of user config.
 *
 * @return USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfigBytes(uint8_t* buffer, uint16_t* length);

/**
 * @brief Load user config from bytes.
 *
 * @param buffer User config bytes.
 * @param length Length of buffer.
 *
 * @return USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfigLoadBytes(uint8_t* buffer, uint16_t length);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
