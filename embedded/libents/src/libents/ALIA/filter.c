/**
 * @file filter.c
 * @brief Implementation of ALIA's statistics (Welford's algorithm)
 *        and event/heartbeat decision logic. See alia.h for the public
 *        interface and full function documentation.
 */
#include "filter.h"
#include <math.h>

#include "../util/time.h"

/** @see welford_init */
void welford_init(WelfordState* state) {
  state->head = 0;
  state->count = 0;
  state->mean = 0.0;
  state->M2 = 0.0;
}

/** @see welford_add */
void welford_add(WelfordState* state, double x) {
  state->sensorMeasurements[state->head] = x;
  state->head = (state->head + 1) % ALIA_STD_DEV_WINDOW_SAMPLES;
  state->count += 1;
  double old_mean = state->mean;
  state->mean += (x - state->mean) / state->count;
  state->M2 += (x - old_mean) * (x - state->mean);
}

/** @see welford_remove */
void welford_remove(WelfordState* state) {
  state->count -= 1;
  double old_mean = state->mean;
  state->mean -=
      (state->sensorMeasurements[state->head] - state->mean) / state->count;
  state->M2 -= (state->sensorMeasurements[state->head] - old_mean) *
               (state->sensorMeasurements[state->head] - state->mean);
}

/** @see welford_push*/
void welford_push(WelfordState* state, double x) {
  if (state->count < ALIA_STD_DEV_WINDOW_SAMPLES) {
    welford_add(state, x);
  } else {
    welford_remove(state);
    welford_add(state, x);
  }
}

/** @see welford_get_stddev */
double welford_get_stddev(const WelfordState* state) {
  if (state->count < 2) {
    return 0.0;
  }
  return sqrt(welford_get_variance(state));
}

/** @see welford_get_mean */
double welford_get_mean(const WelfordState* state) { return state->mean; }

/** @see welford_get_variance */
double welford_get_variance(const WelfordState* state) {
  if (state->count < 2) {
    return 0.0;
  }
  return state->M2 / (state->count - 1);
}

/** @see welford_window_is_full */
bool welford_window_is_full(const WelfordState* state) {
  if (state->count < ALIA_STD_DEV_WINDOW_SAMPLES) {
    return false;
  }
  return true;
}

/** @see alia_startup_complete */
bool alia_startup_complete(const WelfordState* state,
                           const ALIAUserConfig* config) {
  return state->count >= config->num_startup_samples;
}

/** @see alia_registry_init */
void alia_registry_init(ALIARegistry* reg) {
  for (size_t i = 0; i < ALIA_MAX_STREAMS; i++) {
    reg->streams[i].in_use = false;
    reg->streams[i].key = 0;
  }
}

/** @see alia_stream_get */
ALIAStream* alia_stream_get(ALIARegistry* reg, uint32_t key,
                            const ALIAUserConfig* defaults,
                            double sensor_resolution) {
  for (size_t i = 0; i < ALIA_MAX_STREAMS; i++) {
    if (reg->streams[i].in_use && reg->streams[i].key == key) {
      return &reg->streams[i];
    }
  }

  // Not seen before
  for (size_t i = 0; i < ALIA_MAX_STREAMS; i++) {
    if (reg->streams[i].in_use) {
      continue;
    }
    ALIAStream* stream = &reg->streams[i];
    stream->key = key;
    stream->in_use = true;
    welford_init(&stream->welford);
    stream->heartbeat.last_tx_ts = 0;
    stream->heartbeat.last_event_ts = 0;
    stream->heartbeat.has_logged = false;
    stream->heartbeat.last_transmitted_value = 0.0;
    stream->run.run_count = 0;
    stream->config = *defaults;
    stream->config.sensor_resolution = sensor_resolution;
    numSamplesInStartup(&stream->config);
    return stream;
  }

  return NULL;
}

/** @see backoff */
double backoff(HeartbeatState* heartbeatState, ALIAUserConfig* config,
               uint32_t now) {
  double calm_hours = 0.0;

  if (heartbeatState->has_logged) {
    calm_hours = (now - heartbeatState->last_event_ts) / 3600.0;
  }

  if (config->doubling_hours == 0) {
    return config->max_heartbeat_hours;
  }
  uint32_t exponent = calm_hours / config->doubling_hours;
  uint32_t interval = config->base_heartbeat_hours;
  for (uint32_t i = 0; i < exponent && interval < config->max_heartbeat_hours; i++) {
    interval *= 2;
  }
  if (interval < config->max_heartbeat_hours) {
    return interval;
  }
  return config->max_heartbeat_hours;
}

/** @see should_log */
bool should_log(double data, WelfordState* state,
                HeartbeatState* heartbeatState, RunState* runState,
                ALIAUserConfig* config) {
  uint32_t time = epoch();

  // if first value send it
  bool event_fired;
  if (heartbeatState->has_logged) {
    double deviation = fabs(data - heartbeatState->last_transmitted_value);
    double threshold;
    if (alia_startup_complete(state, config)) {
      threshold = welford_get_stddev(state) * config->event_delta_threshold;
      if (threshold < config->sensor_resolution) {
        threshold = config->sensor_resolution;
      }
    } else {
      threshold = config->sensor_resolution;
    }
    double epsilon = 1e-9;
    event_fired = deviation > threshold + epsilon;
  } else {
    event_fired = false;
  }

  bool heartbeat_fired;
  if (!heartbeatState->has_logged) {
    heartbeat_fired = true;
  } else {
    double elapsed_hours = (time - heartbeatState->last_tx_ts) / 3600.0;
    double interval = backoff(heartbeatState, config, time);
    heartbeat_fired = elapsed_hours >= interval;
  }
  bool should_transmit = heartbeat_fired || event_fired;

  welford_push(state, data);

  if (should_transmit) {
    heartbeatState->last_tx_ts = time;
    if (event_fired || !heartbeatState->has_logged) {
      heartbeatState->last_event_ts = time;
    }
    heartbeatState->has_logged = true;
    heartbeatState->last_transmitted_value = data;
    runState->run_count = 0;
  } else {
    runState->run_count += 1;
  }

  return should_transmit;
}
