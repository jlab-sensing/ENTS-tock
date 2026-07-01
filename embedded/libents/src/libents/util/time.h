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
 * @brief Sets the current unix epoch timestamp.
 *
 * @see epoch_to_rtc_date
 *
 * @param epoch Unix epoch.
 */
void set_epoch(uint32_t epoch);

/**
 * @brief Converts tock time to unix epoch time.
 *
 * @param date Pointer to date struct.
 *
 * @returns Unix epoch time.
 */
uint32_t rtc_date_to_epoch(const libtock_rtc_date_t* date);

/**
 * @brief Convert unix epoch to tock time
 *
 * @param epoch Input epoch.
 * @param date Output date
 */
void epoch_to_rtc_date(uint32_t epoch, libtock_rtc_date_t* date);

#ifdef __cplusplus
}
#endif
