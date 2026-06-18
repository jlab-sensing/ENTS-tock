#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <libtock-sync/peripherals/rtc.h>
#include <stdint.h>

/**
 * @brief Gets the current unix epoch timestamp.
 *
 * @see rtc_date_to_epoch
 *
 * @returns Unix epochs.
 */
uint32_t epoch(void);

/**
 * @brief Converts tock time to unix epoch time.
 *
 * @param date Pointer to
 *
 * @returns Unix epoch time.
 */
uint32_t rtc_date_to_epoch(const libtock_rtc_date_t* date);

#ifdef __cplusplus
}
#endif
