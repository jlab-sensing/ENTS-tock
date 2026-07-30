/**
 * @file fm24cl16b.h
 * @brief Driver for FM24CL16B FRAM chip.
 *
 * The FM24CL16B is a 2KB (16 Kbit) FRAM chip with paged I2C addressing.
 * Memory is organized as 8 pages × 256 bytes each.
 * The 11-bit address is split:
 *   - Bits [10:8] are encoded in the device address (A2:A0 bit positions)
 *   - Bits [7:0] are sent as a single-byte memory address
 *
 * This driver follows the same interface pattern as mb85rc1mt.h and uses
 * libtock-c I2C master API for all I2C communication.
 *
 * @author Ahmed Falah
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "fram.h"

/**
 * @defgroup storage Storage
 * @brief Library for interfacing with the FM24CL16B FRAM chip via TockOS
 * @{
 */

/**
 * @brief Write bytes to FRAM at the given address
 *
 * @param addr  Starting address in FRAM (0 to 2047)
 * @param data  Pointer to data to write
 * @param len   Number of bytes to write
 * @return FRAM_OK on success, FRAM_OUT_OF_RANGE or FRAM_ERROR on failure
 */
fram_status fm24cl16b_write(fram_addr addr, const uint8_t* data, size_t len);

/**
 * @brief Read bytes from FRAM at the given address
 *
 * @param addr  Starting address in FRAM (0 to 2047)
 * @param len   Number of bytes to read
 * @param data  Buffer to read into (must be at least len bytes)
 * @return FRAM_OK on success, FRAM_OUT_OF_RANGE or FRAM_ERROR on failure
 */
fram_status fm24cl16b_read(fram_addr addr, size_t len, uint8_t* data);

/**
 * @brief Get the total size of FRAM in bytes
 *
 * @return 2048 (2 KB for FM24CL16B)
 */
fram_addr fm24cl16b_size(void);

/** @} */

#ifdef __cplusplus
}
#endif
