/**
 * @file teros12.h
 * @author John Madden <jmadden173@pm.me>
 * @brief Drivers for reading measurements from Teros12 sensor
 *
 * The implementation is based on the sdi12 example in
 * `libtock-c/examples/sdi12`.
 *
 * @date 2026-08-12
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "../proto/sensor.pb.h"

/**
 * @ingroup sdi12
 * @defgroup teros12 Teros12
 * @brief Library for interfacing with Teros12 sensors
 *
 * This library is designed to read measurements from Teros12 sensors.
 *
 * Datasheet: https://metergroup.com/products/teros-12/
 *
 * Examples:
 * - @ref example_teros12.c
 *
 * @{
 */

typedef struct {
  char addr;
  double vwc;
  double temp;
  unsigned int ec;
} Teros12Data;



/**
 * @brief Get the last measured data.
 *
 * @returns Last measurement
 */
Teros12Data Teros12GetMeasurement(void);


/**
 * @brief Reads measurement from sensor.
 *
 * @param addr Address of the sensor
 *
 * @return SDI12Status
 */
int Teros12Measure(char addr);

/**
 * \defgroup Teros12MeasureGroup Measurement functions for Teros12
 *
 * @brief Gets measurements and encoded them.
 *
 * Since you have to read all fields at once Teros12Measure must be called
 * first to populate the measurement buffer. These functions grab those values
 * and put them in the necessary measurement formats.
 * 
 * @see SensorPrototypeMeasure
 * @{
 */
uint8_t Teros12MeasureVWC(uint8_t* data, Metadata meta, uint32_t idx);
uint8_t Teros12MeasureVWCRaw(uint8_t* data, Metadata meta, uint32_t idx);
uint8_t Teros12MeasureTemp(uint8_t* data, Metadata meta, uint32_t idx);
uint8_t Teros12MeasureEC(uint8_t* data, Metadata meta, uint32_t idx);
/** @} */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
