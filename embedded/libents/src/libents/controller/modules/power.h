/**
 * @file power.h
 * @date 2025-11-04
 * @author John Madden <jmadden173@pm.me>
 * @brief Power interface for the esp32
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup controller
 * @defgroup controllerPower Power
 * @brief Power interface for the esp32
 */

bool ControllerPowerSleep(void);

bool ControllerPowerWakeup(void);

#ifdef __cplusplus
}
#endif
