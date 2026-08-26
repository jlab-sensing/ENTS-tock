/**
 * @file fm24cl16b.c
 * @brief FRAM driver for FM24CL16B using TockOS I2C master syscalls
 *
 * The FM24CL16B is a 2KB (16 Kbit) FRAM with paged I2C interface.
 * Memory is organized as 8 pages × 256 bytes each, accessed via an 11-bit
 * address:
 *   - Bits [10:8] are encoded in the device address (A2:A0 bit positions)
 *   - Bits [7:0] are sent as a single-byte memory address
 *
 * @author Ahmed Falah
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#include "fm24cl16b.h"

#include <libtock/peripherals/i2c_master.h>
#include <string.h>

#include "fram.h"

/** Base device address (1010 0000) */
static const uint8_t FM24CL16B_BASE_ADDR = 0xA0;

/** Total size in bytes */
static const uint32_t FM24CL16B_SIZE = 2048;

/** Page size in bytes */
static const uint32_t FM24CL16B_PAGE_SIZE = 256;

/**
 * @brief Internal representation of the FM24CL16B I2C address
 */
typedef struct {
  /** 8-bit device address (base address | A2:A0 bits) */
  uint8_t dev;
  /** 8-bit memory address within the page */
  uint8_t mem;
} fm24cl16b_address;

/**
 * @brief Convert a flat FRAM address to FM24CL16B I2C address format
 *
 * The 11-bit address is split:
 *   - Bits [10:8] -> device address bits A2:A0 (shifted to positions [3:1])
 *   - Bits [7:0] -> 8-bit memory address
 *
 * @param addr Flat FRAM address (0 to 2047)
 * @return Formatted I2C address
 */
static fm24cl16b_address convert_address(fram_addr addr) {
  fm24cl16b_address i2c_addr;

  // Extract bits [10:8], shift to positions [3:1], OR with base address
  // (addr >> 8) & 0x07 gives us bits [10:8]
  // Left shift by 1 to position them as A2:A0 in device address
  i2c_addr.dev = FM24CL16B_BASE_ADDR | (((addr >> 8) & 0x07) << 1);

  // Extract bits [7:0] for memory address
  i2c_addr.mem = addr & 0xFF;

  return i2c_addr;
}

/**
 * @brief Write a chunk of data to FRAM (handles 32-byte Tock buffer limit)
 *
 * @param addr Starting FRAM address
 * @param data Pointer to data to write
 * @param len Number of bytes to write (must fit within current page)
 * @return FRAM_OK on success, FRAM_ERROR on I2C failure
 */
static fram_status write_chunk(fram_addr addr, const uint8_t* data,
                               size_t len) {
  size_t written = 0;

  while (written < len) {
    fm24cl16b_address i2c_addr = convert_address(addr + written);

    // Build I2C transaction buffer: [mem_addr, data...]
    // Maximum 32 bytes total: 1 byte mem_addr + 30 bytes data
    uint8_t buffer[32];
    buffer[0] = i2c_addr.mem;

    // Calculate chunk size (max 30 data bytes)
    size_t remaining = len - written;
    size_t chunk_size = (remaining > 30) ? 30 : remaining;

    // Copy data into buffer
    memcpy(buffer + 1, data + written, chunk_size);

    // Write chunk via I2C
    int status = i2c_master_write_sync(i2c_addr.dev, buffer, chunk_size + 1);
    if (status < 0) {
      return FRAM_ERROR;
    }

    written += chunk_size;
  }

  return FRAM_OK;
}

/**
 * @brief Read a chunk of data from FRAM (handles page boundaries)
 *
 * @param addr Starting FRAM address
 * @param len Number of bytes to read (must fit within current page)
 * @param data Buffer to read into
 * @return FRAM_OK on success, FRAM_ERROR on I2C failure
 */
static fram_status read_chunk(fram_addr addr, size_t len, uint8_t* data) {
  size_t bytes_read = 0;

  while (bytes_read < len) {
    fm24cl16b_address i2c_addr = convert_address(addr + bytes_read);

    // Set memory address pointer for this page
    uint8_t mem_addr_buf[1] = {i2c_addr.mem};
    int status = i2c_master_write_sync(i2c_addr.dev, mem_addr_buf, 1);
    if (status < 0) {
      return FRAM_ERROR;
    }

    // How many bytes remain in this page from the current address
    size_t page_offset = (addr + bytes_read) % FM24CL16B_PAGE_SIZE;
    size_t bytes_in_page = FM24CL16B_PAGE_SIZE - page_offset;
    size_t chunk =
        (len - bytes_read < bytes_in_page) ? (len - bytes_read) : bytes_in_page;

    // Read chunk byte-by-byte (Tock I2C buffer limit workaround)
    for (size_t i = 0; i < chunk; i++) {
      status = i2c_master_read_sync(i2c_addr.dev, data + bytes_read + i, 1);
      if (status < 0) {
        return FRAM_ERROR;
      }
    }

    bytes_read += chunk;
  }

  return FRAM_OK;
}

fram_status fm24cl16b_write(fram_addr addr, const uint8_t* data, size_t len) {
  // Bounds check
  if (addr + len > FM24CL16B_SIZE) {
    return FRAM_OUT_OF_RANGE;
  }

  size_t bytes_written = 0;

  while (bytes_written < len) {
    // Calculate current page and bytes remaining in current page
    fram_addr current_addr = addr + bytes_written;
    size_t page_offset = current_addr % FM24CL16B_PAGE_SIZE;
    size_t bytes_in_page = FM24CL16B_PAGE_SIZE - page_offset;

    // Determine chunk size for this page
    size_t chunk_len = (len - bytes_written < bytes_in_page)
                           ? (len - bytes_written)
                           : bytes_in_page;

    // Write this chunk (will be further chunked for 32-byte buffer)
    fram_status status =
        write_chunk(current_addr, data + bytes_written, chunk_len);
    if (status != FRAM_OK) {
      return status;
    }

    bytes_written += chunk_len;
  }

  return FRAM_OK;
}

fram_status fm24cl16b_read(fram_addr addr, size_t len, uint8_t* data) {
  // Bounds check
  if (addr + len > FM24CL16B_SIZE) {
    return FRAM_OUT_OF_RANGE;
  }

  size_t bytes_read = 0;

  while (bytes_read < len) {
    // Calculate current page and bytes remaining in current page
    fram_addr current_addr = addr + bytes_read;
    size_t page_offset = current_addr % FM24CL16B_PAGE_SIZE;
    size_t bytes_in_page = FM24CL16B_PAGE_SIZE - page_offset;

    // Determine chunk size for this page
    size_t chunk_len =
        (len - bytes_read < bytes_in_page) ? (len - bytes_read) : bytes_in_page;

    // Read this chunk
    fram_status status = read_chunk(current_addr, chunk_len, data + bytes_read);
    if (status != FRAM_OK) {
      return status;
    }

    bytes_read += chunk_len;
  }

  return FRAM_OK;
}

fram_addr fm24cl16b_size(void) { return FM24CL16B_SIZE; }
