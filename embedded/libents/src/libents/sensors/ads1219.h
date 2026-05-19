/**
 * @file     ads1219.h
 * @author   Stephen Taylor
 * @brief    This file contains all the function prototypes for
 *           the ads.c file
 * @date     11/27/2023
 *
 *
 * This file was copied and modified from the ENTS libraries
 *
 * Copyright [2023] <Stephen Taylor>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

/**
 * @ingroup stm32
 * @defgroup ads1219 ADS1219
 * @brief Library for interfacing with the ADS1219 ADC
 *
 * This library is designed to read measurements from the ADS1219 ADC. When not
 * actively taking measurements, the analog frontend is power down to reduce
 * parasitic current.
 *
 * Library expected I2C and GPIO to be initialized before use.
 *
 * Example: @ref example_adc.c
 *
 * Datasheet: https://www.ti.com/product/ADS1219
 *
 *
 * @{
 */

typedef enum {
  ADS1219_SUCCESS,
  ADS1219_ERROR = -1,
  ADS1219_CONFIGURE = -2,
  ADS1219_MEASURE = -3,
  ADS1219_CONVERSION = -4
} ads1219_status;


/**
* @brief This function starts up the ADS1219
*
* This function is a wrapper for the STM32 HAl I2C library. The ADS1219 uses
* I2C the I2C communication protocol. This function configures then ADS1219 for
* single read mode. Note: the ADS1219 requires a minimum of 500us when it is
* powered on.
*
* @return Return code.
*/
int ads1219_reset(void);

/**
 * @brief This function reads the current ADC voltage value.
 *
 * This function is a wrapper for the STM32 HAl I2C library. The ADS1219 uses
 * I2C the I2C communication protocol. This version simply chops the noisy
 * bits.
 *
 * @param current Pointer to store measurement.
 * @return Return code.
 */
int ads1219_voltage(double *voltage);
int ads1219_voltage_raw(uint32_t *voltage);

/**
 * @brief This function reads the current ADC ampere value.
 *
 * This function is a wrapper for the STM32 HAl I2C library. The ADS1219 uses
 * I2C the I2C communication protocol. This version simply chops the noisy
 * bits.
 *
 * @param current Pointer to store measurement.
 * @return Return code.
 */
int ads1219_current(double *current);
int ads1219_current_raw(uint32_t *current);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
