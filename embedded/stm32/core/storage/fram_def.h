/**
 * @file fram_def.h
 * @author John Madden (jmadden173@pm.me)
 * @brief
 * @version 0.1
 * @date 2024-10-03
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "fram.h"

/**
 * @ingroup storage
 * @defgroup framDef FRAM Definitions
 * @brief Definitions for interfacing with FRAM chips
 * Fram interface definitions and helper functions. Supported fram chips
 * implement read and write functions and expose @ref FramInterface type. See
 * @ref fm24cl16b.h.
 *
 * @{
 */

typedef fram_status (*fram_write_ptr_t)(fram_addr addr, const uint8_t *data,
                                       size_t len);

typedef fram_status (*fram_read_ptr_t)(fram_addr addr, size_t len, uint8_t *data);

typedef fram_addr (*fram_size_ptr_t)(void);

typedef struct {
  /** Pointer to write function */
  fram_write_ptr_t write_ptr;
  /** Pointer to read function */
  fram_read_ptr_t read_ptr;
  /** Size of FRAM */
  fram_size_ptr_t size_ptr;
} fram_interface_t;


/**
 * @}
 */

#ifdef __cplusplus
}
#endif
