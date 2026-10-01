/**
 * @file uptime.h
 * @brief Per device uptime, boot and downtime tracking backed by FRAM.
 *
 * See UPTIME_TRACKING_DESIGN.md at the repo root for the reasoning behind the
 * clock choices and the storage layout. The short version:
 *
 * - Session uptime is accumulated from the Tock alarm tick counter, never from
 *   subtracting two RTC epochs, because timesync can step the RTC mid session.
 * - Everything that has to survive a reset lives in FRAM, which has no erase
 *   cycle and effectively unlimited write endurance.
 * - Downtime is computed only after a successful timesync, because on the
 *   measured hardware the RTC resets to 2000-01-01 and is useless at boot.
 *
 * Typical use from an application:
 *
 * @code
 * ents_uptime_init();                 // early, before the network
 * ...
 * lorawan_timesync();
 * ents_uptime_time_synced();          // now downtime can be measured
 * ...
 * ents_uptime_tick();                 // periodically, from an alarm
 * ...
 * ents_uptime_mark_clean();           // before a graceful shutdown
 * @endcode
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @ingroup util
 * @defgroup uptime Uptime
 * @brief Boot counting, session uptime and measured downtime
 * @{
 */

#ifndef ENTS_UPTIME_ADDR_A
/** First record slot. Well clear of FRAM_BUFFER_END so the measurement FIFO can
 * grow without colliding. Verified on hardware at this address. */
#define ENTS_UPTIME_ADDR_A 0x1F000
#endif

#ifndef ENTS_UPTIME_ADDR_B
/** Second record slot. Writes alternate between the two so a torn write during
 * a brownout cannot destroy the only copy. */
#define ENTS_UPTIME_ADDR_B 0x1F100
#endif

#ifndef ENTS_UPTIME_PERSIST_INTERVAL_S
/**
 * How often ents_uptime_tick() writes the record back to FRAM.
 *
 * This sets the resolution of the downtime measurement: an outage is timed from
 * the last persisted heartbeat, so downtime is overstated by up to this much.
 * Five minutes costs 12 FRAM writes an hour, which is nothing against the
 * part's endurance, and is far finer than any realistic upload interval.
 */
#define ENTS_UPTIME_PERSIST_INTERVAL_S 300
#endif

/** Status codes. */
typedef enum {
  ENTS_UPTIME_OK = 0,
  /** FRAM read or write failed. */
  ENTS_UPTIME_ERR_FRAM = -1,
  /** Called before ents_uptime_init(). */
  ENTS_UPTIME_ERR_NOT_INIT = -2,
  /** The alarm driver would not report its frequency. */
  ENTS_UPTIME_ERR_NO_CLOCK = -3,
} ents_uptime_status;

/** Snapshot of the counters, for reporting. */
typedef struct {
  /** Boots since the record was first created, including this one. */
  uint32_t boot_count;
  /** Boots that followed a reset or brownout rather than a clean shutdown. */
  uint32_t unclean_boots;
  /** Seconds since this boot. */
  uint32_t session_seconds;
  /** Seconds summed over every boot. */
  uint32_t cumulative_seconds;
  /** Seconds spent powered off, summed over every measured outage. */
  uint32_t downtime_seconds;
  /** True once a timesync has been reported this boot. */
  bool time_valid;
  /** True if the previous shutdown set the clean flag. */
  bool previous_clean;
} ents_uptime_stats;

/**
 * @brief Load the record, count this boot, and start the session timer.
 *
 * Call once, early, before the network comes up. Safe to call when no valid
 * record exists yet; that is treated as the first ever boot.
 *
 * Marks the record dirty, so if the node resets without ever calling
 * ents_uptime_mark_clean() the next boot sees an unclean restart. That is the
 * mechanism that detects brownouts and watchdog resets.
 *
 * Does not attempt to measure downtime. At this point the RTC is usually still
 * at its power on value, so any subtraction would be garbage.
 *
 * @see ents_uptime_time_synced
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_init(void);

/**
 * @brief Fold elapsed ticks into the counters. Does no I/O.
 *
 * Safe to call from an alarm callback, unlike ents_uptime_tick(), because it
 * never touches FRAM and therefore never starts an i2c transaction.
 *
 * The point of having this separately is the tick counter wrap. The counter is
 * 32 bits and rolls over every 74 hours on the measured hardware, so something
 * has to sample it on a timer even when the application's main loop is blocked
 * waiting for data. Put this on a repeating alarm and leave the persisting to
 * ents_uptime_tick() in the main loop.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_accumulate(void);

/**
 * @brief Accumulate elapsed time and periodically persist the heartbeat.
 *
 * Writes to FRAM, so call this from normal application context rather than
 * from a callback. See ents_uptime_accumulate() for the callback safe half.
 *
 * Call from a repeating alarm. Two things depend on the cadence:
 *
 * - The tick counter is 32 bits and wraps, measured at 74 hours on the bench
 *   stm32wle5jc. This must be called more than once per wrap period or elapsed
 *   time is silently lost.
 * - The persisted heartbeat sets the downtime resolution, see
 *   ENTS_UPTIME_PERSIST_INTERVAL_S.
 *
 * Cheap to call often. Only every ENTS_UPTIME_PERSIST_INTERVAL_S seconds does
 * it actually touch FRAM.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_tick(void);

/**
 * @brief Report that the clock is now trustworthy, and measure any downtime.
 *
 * Call right after a successful timesync. This is where downtime is computed,
 * not at boot, because the stored heartbeat from the previous boot can only be
 * compared against a clock that has actually been set.
 *
 * If the previous boot never reached a timesync there is nothing to compare
 * against, and the outage is counted as a restart with unmeasurable duration.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_time_synced(void);

/**
 * @brief Record that the node is shutting down deliberately.
 *
 * Sets the clean flag and flushes to FRAM, so the next boot does not count an
 * unclean restart. Anything that resets without calling this, which includes
 * every brownout and watchdog reset, leaves the flag clear.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_mark_clean(void);

/**
 * @brief Force the current counters out to FRAM.
 *
 * ents_uptime_tick() already does this on its own schedule. Use this before
 * something risky, or when a fresh heartbeat matters more than the write.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_flush(void);

/**
 * @brief Read the current counters.
 *
 * @param out Receives the snapshot. Must not be NULL.
 * @return ENTS_UPTIME_OK on success, ENTS_UPTIME_ERR_NOT_INIT if
 * ents_uptime_init() has not run.
 */
ents_uptime_status ents_uptime_get(ents_uptime_stats* out);

/**
 * @brief Wipe both record slots.
 *
 * Intended for provisioning a node or resetting a bench board. The next
 * ents_uptime_init() will look like a first ever boot.
 *
 * @return ENTS_UPTIME_OK on success.
 */
ents_uptime_status ents_uptime_erase(void);

/** @} */

#ifdef __cplusplus
}
#endif
