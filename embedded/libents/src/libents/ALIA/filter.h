/**
 * @file filter.h
 * @brief Public interface and data structures for ALIA (Adaptive
 *        Level-Crossing Interval Aggregator), an event-driven adaptive
 *        reporting algorithm for wireless sensor networks.
 *
 * @details
 * ALIA decides, per sensor reading, whether a value is worth transmitting.
 * It combines:
 *  - A **level-crossing / send-on-delta event trigger**, using an
 *    estimate (Welford) of recent signal standard deviation to set an
 *    adaptive deviation threshold, floored by sensor resolution.
 *  - An **adaptive heartbeat trigger**, whose interval exponentially
 *    backs off the longer the signal stays "calm," up to a configured
 *    maximum.
 *
 * This header exposes the configuration struct, the transmit-record
 * format, the statistics state (WelfordState) used to compute the
 * adaptive threshold, and the core decision/backoff functions
 * implemented in filter.c.
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#ifndef ALIA_H
#define ALIA_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @def ALIA_STD_DEV_WINDOW_SAMPLES
 * @brief Fixed capacity, in samples, of the sliding window used to
 *        estimate the signal's rolling standard deviation.
 */
#define ALIA_STD_DEV_WINDOW_SAMPLES 144

/**
 * @struct ALIAUserConfig
 * @brief User-tunable configuration parameters for an ALIA instance.
 *
 * @details
 * These parameters control both of ALIA's triggers: the event-detection
 * sensitivity (sensor_resolution, event_delta_threshold,
 * std_dev_window_hours) and the heartbeat backoff schedule
 * (base_heartbeat_hours, doubling_hours, max_heartbeat_hours).
 */
typedef struct ALIAUserConfig {
  uint32_t sample_rate; /**< Sampling *period*, in seconds between samples, used
                           to size the startup window.  */
  double sensor_resolution; /**< Fixed sensor precision; floors the adaptive
                               event threshold so noise below this level never
                               triggers a report. */
  uint32_t
      event_delta_threshold; /**< Multiplier applied to the rolling stddev to
                                compute the event-detection threshold. */
  uint32_t std_dev_window_hours; /**< Duration, in hours, that the stddev
                                    sliding window should represent; used to
                                    derive num_startup_samples. */
  uint32_t base_heartbeat_hours; /**< Initial (minimum) heartbeat interval, in
                                    hours, used immediately after an event. */
  uint32_t doubling_hours; /**< Number of calm hours after which the heartbeat
                              interval doubles. */
  uint32_t
      max_heartbeat_hours;      /**< Upper bound on the heartbeat interval,
                                   regardless of how long the signal stays calm. */
  uint32_t num_startup_samples; /**< Number of samples needed to fill the stddev
                                   window for std_dev_window_hours; computed by
                                   numSamplesInStartup(). */
} ALIAUserConfig;

/**
 * @struct ALIATransmitRecord
 * @brief Represents a single transmitted (reported) value and the run
 *        of samples it represents.
 *
 * @details
 * Since ALIA suppresses "uninteresting" readings rather than sending
 * every sample, a transmitted record implicitly stands in for
 * runLength consecutive samples until the next transmission.
 */
typedef struct {
  float value;         // the data value being reported
  uint32_t runLength;  // how many samples this value/run represents
  uint32_t timestamp;  // epoch() at time of transmission
} ALIATransmitRecord;

/**
 * @fn numSamplesInStartup
 * @brief Calculates the number of samples needed to fill the user configured
 * std dev window for std_dev_window_hours at a set sample_rate
 *
 * @details
 * Until this many samples have been collected, the rolling window std dev is
 * not representative of std_dev_window_hours of data, so should_log() falls
 * back to the sensor_resolution floor for its event threshold.
 */
// calculate number of startup samples needed for stdDevWindowHours
static inline void numSamplesInStartup(struct ALIAUserConfig* cfg) {
  if (cfg->sample_rate == 0) {
    cfg->num_startup_samples = 0;
    return;
  }
  uint32_t needed = (cfg->std_dev_window_hours * 3600) / cfg->sample_rate;
  if (needed > ALIA_STD_DEV_WINDOW_SAMPLES) {
    needed = ALIA_STD_DEV_WINDOW_SAMPLES;
  }
  cfg->num_startup_samples = needed;
}

/**
 * @struct RunState
 * @brief Represents the length of a run when run length encoding is utilized.
 */
typedef struct RunState {
  uint32_t run_count;
} RunState;

/**
 * @struct HeartbeatState
 * @brief Tracks the state needed to evaluate both ALIA triggers.
 *
 * @details
 * The two timestamps are deliberately distinct. @ref last_tx_ts answers "how
 * long since we last said anything?" and so gates the heartbeat. @ref
 * last_event_ts answers "how long has the signal been calm?" and so drives the
 * exponential backoff.
 */
typedef struct HeartbeatState {
  /** epoch() of the most recent transmission of any kind (event or
   *  heartbeat). Drives the heartbeat elapsed-time check. */
  uint32_t last_tx_ts;
  /** epoch() of the most recent *event-triggered* transmission. Drives
   *  backoff(); deliberately NOT updated by heartbeat-only transmissions. */
  uint32_t last_event_ts;
  bool has_logged;
  double last_transmitted_value;
} HeartbeatState;

/**
 * @struct WelfordState
 * @brief Online (constant-memory) statistics state for computing the
 *        mean/variance/standard deviation of a fixed-size sliding
 *        window of recent sensor readings, via Welford's algorithm.
 *
 * @details
 * Backs ALIA's adaptive event-detection threshold: the standard
 * deviation of the current window scales how large a deviation must be
 * before a reading is considered "interesting" enough to transmit.
 */
typedef struct {
  size_t head;
  size_t count;
  double mean;
  double M2;
  double sensorMeasurements[ALIA_STD_DEV_WINDOW_SAMPLES];
} WelfordState;

/**
 * @brief Initializes a WelfordState for a fresh sliding window.
 * @param state Pointer to the WelfordState to initialize; all fields
 *              are zeroed.
 */
void welford_init(WelfordState* state);

/**
 * @brief Pushes a new sample into the fixed-size sliding window,
 *        evicting the oldest sample once full.
 * @param state Pointer to the WelfordState to update.
 * @param x New sensor sample to incorporate.
 */
void welford_push(WelfordState* state, double x);

/**
 * @brief Adds a new sample to the rolling window statistics without
 *        evicting anything (used internally while the window is still
 *        filling).
 * @param state Pointer to the WelfordState to update.
 * @param x New sensor sample to incorporate.
 */
void welford_add(WelfordState* state, double x);

/**
 * @brief Removes the oldest sample from the rolling window statistics.
 *
 * @note Declared here as `welford_remove` to match its definition in
 *       filter.c — the header previously had this misspelled as
 *       `wleford_remove`, which would fail to link against filter.c.
 *
 * @param state Pointer to the WelfordState to update.
 */
void welford_remove(WelfordState* state);

/**
 * @brief Returns the standard deviation of the current sliding window.
 * @param state Pointer to the WelfordState to query.
 * @return Standard deviation, or 0.0 if fewer than 2 samples are held.
 */
double welford_get_stddev(const WelfordState* state);

/**
 * @brief Returns the mean of the current sliding window.
 * @param state Pointer to the WelfordState to query.
 * @return Mean of samples currently in the window.
 */
double welford_get_mean(const WelfordState* state);

/**
 * @brief Returns the variance of the current sliding window.
 * @param state Pointer to the WelfordState to query.
 * @return Variance of samples currently in the window.
 */
double welford_get_variance(const WelfordState* state);

/**
 * @brief Returns whether the sliding window has reached full capacity.
 * @param state Pointer to the WelfordState to query.
 * @return true if the sliding window contains ALIA_STD_DEV_WINDOW_SAMPLES
 */
bool welford_window_is_full(const WelfordState* state);

/**
 * @brief Returns whether enough samples have been collected for the rolling
 *        stddev to represent config->std_dev_window_hours of data.
 *
 * @details
 * Until this returns true, should_log() ignores the (unrepresentative) stddev
 * and uses config->sensor_resolution as the event threshold.
 * @param state Pointer to the WelfordState to query.
 * @param config Pointer to the user-configured ALIA parameters.
 * @return true once state->count >= config->num_startup_samples.
 */
bool alia_startup_complete(const WelfordState* state,
                           const ALIAUserConfig* config);

/**
 * @brief Global ALIA configuration instance.
 *
 * @details
 * Declared extern so it can be defined once elsewhere and exposed for
 * runtime configuration (e.g. via the ESP32 WiFi config page).
 */
extern struct ALIAUserConfig global_ALIAConfig;

/**
 * @brief Core ALIA decision function: determines whether a new sensor
 *        reading should be transmitted, combining the event and
 *        heartbeat triggers.
 *
 * @details
 * Owns all of heartbeatState. On a transmit decision it records the reading as
 * the new baseline in heartbeatState->last_transmitted_value; callers must not
 * maintain that field themselves. Always feeds @p data into the rolling
 * window, transmitted or not, so the stddev reflects the true signal rather
 * than only the reported subset.
 *
 * @param data New sensor reading to evaluate.
 * @param state Pointer to the Welford sliding-window statistics state.
 * @param heartbeatState Pointer to the heartbeat/backoff state, updated
 *                        in place.
 * @param runState Pointer to the run-length state, updated in place.
 * @param config Pointer to the user-configured ALIA parameters.
 * @return true if this reading should be transmitted, false otherwise.
 */
bool should_log(double data, WelfordState* state,
                HeartbeatState* heartbeatState, RunState* runState,
                ALIAUserConfig* config);

/**
 * @brief Computes the current adaptive heartbeat interval, in hours,
 *        given how long the signal has stayed calm.
 *
 * @param heartbeatState Pointer to the current heartbeat/backoff state.
 * @param config Pointer to the user-configured ALIA parameters.
 * @param now Current time (epoch seconds).
 * @return Heartbeat interval to wait before the next forced
 *         transmission, in hours, capped at max_heartbeat_hours.
 */
double backoff(HeartbeatState* heartbeatState, ALIAUserConfig* config,
               uint32_t now);
#endif
#ifdef __cplusplus
}
#endif