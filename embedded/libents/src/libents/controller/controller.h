/**
 * @file controller.h
 * @date 2024-10-13
 * @author John Madden <jmadden173@pm.me>
 * @brief Controller module for stm32
 *
 */

/**
 * @ingroup stm32
 * @defgroup controller Controller
 * @brief Controller library for communication between stm32 and esp32
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup controller
 * @defgroup controllerInterface Controller Interface
 * @brief Interface for the controller module
 * @{
 */

/**
 * @brief Shared initialization for all esp32  modules
 *
 * Allocates memory to the tx and rx buffers.
 *
 * @todo Add check for communication with the esp32
 */
void ControllerInit(void);

/**
 * @brief Shared deinitialize for all esp32 modules
 *
 * Free memory associated with the tx and rx buffers.
 */
void ControllerDeinit(void);

/**
 * @brief Trigger esp32 wakeup pin
 *
 * After this function is called and the controller is initialized with
 * ControllerInit(), the function ControllerPowerWakeup() should be used to
 * check that the esp32 has correctly woken up.
 *
 * Since this is blocking code, a small internal delay is added for the startup
 * time on the esp32.
 */
void ControllerWakeup(void);

/**
 * @brief Resets the controller interface.
 *
 * Toggles the enable pin for the esp32.
 *
 * 100ms delay between off and on seems to be sufficient.
 *
 * A delay is necessary to give the ESP32 enough time to initialize.
 * Not recommended to go below 500 ms. ESP32 may fail to receive a
 * subsequent command when there is <500 ms delay between ESP32_EN
 * being enabled and the next I2C transaction with the ESP32.
 */
void ControllerDeviceReset(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
