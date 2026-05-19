/**
 * @file power.h
 * @date 2025-11-04
 * @author John Madden <jmadden173@pm.me>
 * @brief Power interface for the esp32
 */

#pragma once

#include <stdbool.h>

/**
 * @ingroup controller
 * @defgroup controllerPower Power
 * @brief Power interface for the esp32
 */

bool ControllerPowerSleep(void);

bool ControllerPowerWakeup(void);
