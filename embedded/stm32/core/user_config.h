/**
 * @file user_config.h
 * @author John Madden
 * @brief Controls the user config webserver
 * @date 2025-10-01
 *
 * This files interfaces with the user config webserver. It allows starting and
 * stopping of the webserver. There is an optional timeout to stop the server
 * after a period of inactivity (no clients connected).
 */

#pragma once

#include <cstdint>

typedef enum {
  USERCONFIG_STATE_OFF,
  USERCONFIG_STATE_ON,
  USERCONFIG_STATE_STOPPING
} UserConfigState;


/**
 * @brief Starts the user config webserver
 *
 * If retry_ms is 0, then autostop is disabled. Use UserConfigStop() to
 * initiate a stop.
 *
 * @param retry_ms Time between webserver stop attempts.
 */
void UserConfigStart(uint32_t retry_ms);

/**
 * @brief Stops the user config webserver
 *
 * Schedule the webserver to be stopped. The webesrver can be prevented from
 * stopping if there is still a client connected. The argument retry_ms
 * specifies how long to go between stop attempts.
 *
 * @param retry_ms Time between webserver stop attempts.
 */
void UserConfigStop(uint32_t retry_ms);

/**
 * @brief Get the current user config state.
 *
 * @see UserConfigStates
 *
 * @return Current state.
 */
int UserConfigCurrentState(void);
