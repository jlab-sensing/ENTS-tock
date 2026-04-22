/**
 * @file mb85rc1mt.c
 * @brief FRAM driver for MB85RC1MT using TockOS I2C master syscalls
 *
 * Ported from stm32/lib/storage/src/mb85rc1mt.c.
 * Replaces HAL_I2C_Mem_Write/Read with libtock-c i2c_master_write_sync
 * and i2c_master_write_read_sync.
 *
 * The MB85RC1MT is a 128KB (1 Mbit) FRAM with I2C interface.
 * Memory is accessed via a 17-bit address:
 *   - Bit 16 is encoded in the device address (A0 bit position)
 *   - Bits 15:0 are sent as a 16-bit memory address
 *
 * @author Pritish Mahato
 * @date 2026-02-25
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#include "mb85rc1mt.h"

#include <string.h>
#include <libtock/peripherals/i2c_master.h>

#include "fram.h"

/** Base address of chip */
static const uint8_t g_base_addr = 0b10100000;

/** Number of pages on the chip */
static const int mb85rc1mt_pages = 1;

/** Size of each memory segment in bytes */
static const int mb85rc1mt_seg_size = 1 << 17;

/**
 * @brief Internal representation of the MB85RC1MT I2C address
 */
typedef struct {
  /** 8-bit device address (includes upper address bit and R/W) */
  uint8_t dev;
  /** 16-bit memory address within the device */
  uint16_t mem;
} mb85rc1mt_address;


/**
 * @brief Put the chip into sleep mode
 *
 */
fram_status mb85rc1mt_wakeup(mb85rc1mt_address addr);


/**
 * @brief Wake up device from sleep
 *
 * @return
 */
fram_status mb85rc1mt_sleep(mb85rc1mt_address addr);



/**
 * @brief Convert a flat FRAM address to MB85RC1MT I2C address format
 *
 * The 17-bit address is split:
 *   - Bit 16 -> device address bit A0 (shifted into position)
 *   - Bits 15:0 -> 16-bit memory address
 *
 * @param addr Flat FRAM address (0 to MB85RC1MT_SIZE-1)
 * @return Formatted I2C address
 */
static mb85rc1mt_address convert_address(fram_addr addr) {
  mb85rc1mt_address i2c_addr;

  // Upper address bits go into device address.
  // Shift right by 15 to get bit 16 into position, mask to keep only
  // relevant bits, OR with base address.
  i2c_addr.dev = g_base_addr | ((addr >> 15) & 0x0E);
  i2c_addr.mem = addr & 0xFFFF;

  return i2c_addr;
}

fram_status mb85rc1mt_write(fram_addr addr, const uint8_t* data, size_t len) {
  fram_status status = FRAM_OK;

  while (len > 0) {
    // convert fram address to i2c address
    mb85rc1mt_address i2c_addr = convert_address(addr);

    // wakeup device
    status = mb85rc1mt_wakeup(i2c_addr);
    if (status != FRAM_OK) {
      return status;
    }

    // number of bytes that can be written without changing address
    size_t write_len = 0;
    if ((size_t)addr + len > mb85rc1mt_seg_size) {
      write_len = mb85rc1mt_seg_size - addr;
    } else {
      write_len = len;
    }

    for (size_t i = 0; i < write_len; i++) {
      // transmit data
      int tock_status = RETURNCODE_SUCCESS;

      uint8_t buffer[write_len + 2] = {};
      buffer[0] = (uint8_t)(i2c_addr.mem >> 8);
      buffer[1] = (uint8_t)(i2c_addr.mem & 0xFF);
      memcpy(buffer+2, data, write_len);
      tock_status = i2c_master_write_sync(i2c_addr.dev, buffer, write_len + 2);

      if (tock_status < 0) {
        return FRAM_OK;
      }
    }

    // update address and length
    addr += write_len;
    len -= write_len;
    data += write_len;

    // sleep device
    status = mb85rc1mt_sleep(i2c_addr);
    if (status != FRAM_OK) {
      return status;
    }
  }

  return status;
}

fram_status mb85rc1mt_read(fram_addr addr, size_t len, uint8_t* data) {
  fram_status status = FRAM_OK;

  while (len > 0) {
    // convert fram address to i2c address
    mb85rc1mt_address i2c_addr = convert_address(addr);

    // wakeup device
    status = mb85rc1mt_wakeup(i2c_addr);
    if (status != FRAM_OK) {
      return status;
    }

    size_t read_len = 0;
    if ((size_t)addr + len > mb85rc1mt_seg_size) {
      read_len = mb85rc1mt_seg_size - addr;
    } else {
      read_len = len;
    }

    // transmit data
    int tock_status = RETURNCODE_SUCCESS;

    data[0] = (uint8_t) (i2c_addr.mem >> 8);
    data[0] = (uint8_t) (i2c_addr.mem & 0xFF);
    tock_status = i2c_master_write_read_sync(i2c_addr.dev, data, 2, read_len);
    if (tock_status < 0) {
      return FRAM_ERROR;
    }

    // update read params
    addr += read_len;
    len -= read_len;
    data += read_len;

    // sleep device
    status = mb85rc1mt_sleep(i2c_addr);
    if (status != FRAM_OK) {
      return status;
    }
  }

  // return status
  return status;
}

fram_status mb85rc1mt_wakeup(mb85rc1mt_address addr) {
  // TODO: Implement wakeup

  (void) addr;

  return FRAM_OK;
}

fram_status mb85rc1mt_sleep(mb85rc1mt_address addr) {
  // TODO: Implement sleep

  (void) addr;

  return FRAM_OK;
}

fram_addr mb85rc1mt_size(void) {
  return mb85rc1mt_pages * mb85rc1mt_seg_size;
}
