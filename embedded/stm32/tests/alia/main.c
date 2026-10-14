/*
 * @file main.c
 * @brief Test application for ALIA (Adaptive Level-crossing Interval Algorithm)
 *
 * Exercises basic functionality of algorithm
 * using the Unity testing framework.
 *
 * Based on stm32/test/fifo/main.c from ENTS-tock.
 *
 * @author Alec Levy
 * @date 2026-08-17
 *
 * Copyright (c) 2026 jLab, UCSC
 */
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <libents/ALIA/filter.h>
#include <math.h>
#include <stdbool.h>
#include <unity.h>

#define ASSERT_DOUBLE_EQUAL(expected, actual) \
  TEST_ASSERT_TRUE(fabs((expected) - (actual)) <= 1e-9)
#define ASSERT_DOUBLE_WITHIN(delta, expected, actual) \
  TEST_ASSERT_TRUE(fabs((expected) - (actual)) <= (delta))

WelfordState welfordState;
HeartbeatState heartbeatState;
RunState runState;
ALIAUserConfig config;

void setUp(void) {
  welford_init(&welfordState);
  heartbeatState.last_tx_ts = 0;
  heartbeatState.last_event_ts = 0;
  heartbeatState.has_logged = false;
  heartbeatState.last_transmitted_value = 0.0;
  runState.run_count = 0;
  config.event_delta_threshold = 2;
  config.sensor_resolution = 0.1;
  config.base_heartbeat_hours = 1;
  config.doubling_hours = 6;
  config.max_heartbeat_hours = 24;
  config.sample_rate = 0;
  config.std_dev_window_hours = 0;
  config.num_startup_samples = 0;
}

void tearDown(void) {}

// ---- naive reference helpers, used to sanity-check the incremental math ----

static double naive_mean(const double* values, size_t n) {
  double sum = 0.0;
  for (size_t i = 0; i < n; i++) {
    sum += values[i];
  }
  return sum / (double)n;
}

static double naive_sample_variance(const double* values, size_t n) {
  if (n < 2) {
    return 0.0;
  }
  double mean = naive_mean(values, n);
  double sum_sq_diff = 0.0;
  for (size_t i = 0; i < n; i++) {
    double diff = values[i] - mean;
    sum_sq_diff += diff * diff;
  }
  return sum_sq_diff / (double)(n - 1);
}

// ==== welford_init ====

void test_welford_init_ZeroState(void) {
  welford_init(&welfordState);
  TEST_ASSERT_EQUAL(0, welfordState.count);
  TEST_ASSERT_EQUAL(0, welfordState.head);
  ASSERT_DOUBLE_EQUAL(0.0, welfordState.mean);
  ASSERT_DOUBLE_EQUAL(0.0, welfordState.M2);
}

// ==== welford_push: fill phase ====

void test_welford_push_SingleValue_MeanEqualsValue(void) {
  welford_push(&welfordState, 5.0);
  ASSERT_DOUBLE_EQUAL(5.0, welford_get_mean(&welfordState));
  ASSERT_DOUBLE_EQUAL(0.0, welford_get_variance(&welfordState));
}

void test_welford_push_KnownSequence_MatchesNaiveRecompute(void) {
  double values[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
  size_t n = sizeof(values) / sizeof(values[0]);

  for (size_t i = 0; i < n; i++) {
    welford_push(&welfordState, values[i]);
  }

  double expected_mean = naive_mean(values, n);
  double expected_variance = naive_sample_variance(values, n);

  ASSERT_DOUBLE_WITHIN(1e-9, expected_mean, welford_get_mean(&welfordState));
  ASSERT_DOUBLE_WITHIN(1e-9, expected_variance,
                       welford_get_variance(&welfordState));
}

void test_welford_push_ConstantValues_VarianceIsZero(void) {
  for (int i = 0; i < 10; i++) {
    welford_push(&welfordState, 3.14);
  }
  ASSERT_DOUBLE_WITHIN(1e-9, 0.0, welford_get_variance(&welfordState));
}

// ==== welford_push: steady-state phase (window full, evicting) ====
void test_welford_push_PastCapacity_MatchesNaiveSlidingWindow(void) {
  int total = ALIA_STD_DEV_WINDOW_SAMPLES + 50;
  double history[ALIA_STD_DEV_WINDOW_SAMPLES + 50];

  for (int i = 0; i < total; i++) {
    double v = (double)(i % 17) + 0.5;
    history[i] = v;
    welford_push(&welfordState, v);

    int window_start = (i + 1 > ALIA_STD_DEV_WINDOW_SAMPLES)
                           ? (i + 1 - ALIA_STD_DEV_WINDOW_SAMPLES)
                           : 0;
    int window_len = (i + 1) - window_start;

    double expected_mean = naive_mean(&history[window_start], window_len);
    double expected_variance =
        naive_sample_variance(&history[window_start], window_len);

    ASSERT_DOUBLE_WITHIN(1e-6, expected_mean, welford_get_mean(&welfordState));
    ASSERT_DOUBLE_WITHIN(1e-6, expected_variance,
                         welford_get_variance(&welfordState));
  }
}

void test_welford_push_HeadWrapsCorrectly(void) {
  for (int i = 0; i < ALIA_STD_DEV_WINDOW_SAMPLES; i++) {
    welford_push(&welfordState, (double)i);
  }
  TEST_ASSERT_EQUAL(0, welfordState.head);

  welford_push(&welfordState, 999.0);
  TEST_ASSERT_EQUAL(1, welfordState.head);
  ASSERT_DOUBLE_EQUAL(999.0, welfordState.sensorMeasurements[0]);
}

void test_welford_push_ConstantValues_PastCapacity_VarianceStaysZero(void) {
  for (int i = 0; i < ALIA_STD_DEV_WINDOW_SAMPLES + 20; i++) {
    welford_push(&welfordState, 7.0);
  }
  ASSERT_DOUBLE_WITHIN(1e-9, 0.0, welford_get_variance(&welfordState));
}

// ==== welford_get_stddev / welford_get_variance edge cases ====

void test_welford_get_variance_LessThanTwoSamples_ReturnsZero(void) {
  welford_push(&welfordState, 1.0);
  ASSERT_DOUBLE_EQUAL(0.0, welford_get_variance(&welfordState));
}

void test_welford_get_stddev_MatchesSqrtOfVariance(void) {
  double values[] = {1.0, 3.0, 5.0, 7.0};
  for (size_t i = 0; i < 4; i++) {
    welford_push(&welfordState, values[i]);
  }
  double expected = sqrt(welford_get_variance(&welfordState));
  ASSERT_DOUBLE_WITHIN(1e-9, expected, welford_get_stddev(&welfordState));
}

// ==== welford_window_is_full ====

void test_welford_window_is_full_FalseBeforeCapacity(void) {
  for (int i = 0; i < ALIA_STD_DEV_WINDOW_SAMPLES - 1; i++) {
    welford_push(&welfordState, (double)i);
  }
  TEST_ASSERT_FALSE(welford_window_is_full(&welfordState));
}

void test_welford_window_is_full_TrueAtCapacity(void) {
  for (int i = 0; i < ALIA_STD_DEV_WINDOW_SAMPLES; i++) {
    welford_push(&welfordState, (double)i);
  }
  TEST_ASSERT_TRUE(welford_window_is_full(&welfordState));
}

// ==== backoff ====

void test_backoff_NotYetLogged_ReturnsBaseHeartbeat(void) {
  heartbeatState.has_logged = false;
  double result = backoff(&heartbeatState, &config, 1000);
  ASSERT_DOUBLE_EQUAL(config.base_heartbeat_hours, result);
}

void test_backoff_ZeroElapsed_ReturnsBaseHeartbeat(void) {
  heartbeatState.has_logged = true;
  heartbeatState.last_event_ts = 1000;
  double result = backoff(&heartbeatState, &config, 1000);
  ASSERT_DOUBLE_EQUAL(config.base_heartbeat_hours, result);
}

void test_backoff_GrowsWithElapsedTime(void) {
  heartbeatState.has_logged = true;
  heartbeatState.last_event_ts = 0;
  uint32_t now = config.doubling_hours * 3600;
  double result = backoff(&heartbeatState, &config, now);
  ASSERT_DOUBLE_WITHIN(1e-6, config.base_heartbeat_hours * 2.0, result);
}

void test_backoff_CapsAtMaxHeartbeat(void) {
  heartbeatState.has_logged = true;
  heartbeatState.last_event_ts = 0;
  uint32_t now = config.doubling_hours * 3600 * 20;  // many doubling periods
  double result = backoff(&heartbeatState, &config, now);
  ASSERT_DOUBLE_EQUAL(config.max_heartbeat_hours, result);
}

// ==== should_log ====

void test_should_log_FirstCall_ReturnsTrue(void) {
  bool result =
      should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_TRUE(heartbeatState.has_logged);
}

void test_should_log_NoEventNoHeartbeat_ReturnsFalse(void) {
  for (int i = 0; i < 5; i++) {
    should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  }
  uint32_t prev_run_count = runState.run_count;
  bool result =
      should_log(50.1, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_EQUAL(prev_run_count + 1, runState.run_count);
}

void test_should_log_LargeDeviation_TriggersEvent(void) {
  for (int i = 0; i < 5; i++) {
    should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  }
  bool result =
      should_log(9999.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);
}

void test_should_log_HeartbeatElapsed_TriggersEvenWithoutDeviation(void) {
  should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  uint32_t age = config.max_heartbeat_hours * 3600 + 1;
  heartbeatState.last_tx_ts -= age;
  heartbeatState.last_event_ts -= age;
  bool result =
      should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);
}

// ==== regression: heartbeats must not reset the backoff clock ====

void test_should_log_HeartbeatDoesNotResetBackoffClock(void) {
  should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  uint32_t seeded_event_ts = heartbeatState.last_event_ts;

  uint32_t age = config.max_heartbeat_hours * 3600 + 1;
  heartbeatState.last_tx_ts -= age;
  heartbeatState.last_event_ts -= age;
  uint32_t aged_event_ts = heartbeatState.last_event_ts;

  bool result =
      should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);

  TEST_ASSERT_EQUAL_UINT32(aged_event_ts, heartbeatState.last_event_ts);
  TEST_ASSERT_NOT_EQUAL(aged_event_ts, heartbeatState.last_tx_ts);
  (void)seeded_event_ts;
}

void test_should_log_EventResetsBackoffClock(void) {
  should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  uint32_t age = 10 * 3600;
  heartbeatState.last_tx_ts -= age;
  heartbeatState.last_event_ts -= age;
  uint32_t aged_event_ts = heartbeatState.last_event_ts;

  bool result =
      should_log(9999.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_NOT_EQUAL(aged_event_ts, heartbeatState.last_event_ts);
  TEST_ASSERT_EQUAL_UINT32(heartbeatState.last_tx_ts,
                           heartbeatState.last_event_ts);
}

// ==== regression: should_log owns last_transmitted_value ====

void test_should_log_UpdatesLastTransmittedValue(void) {
  should_log(42.5, &welfordState, &heartbeatState, &runState, &config);
  ASSERT_DOUBLE_EQUAL(42.5, heartbeatState.last_transmitted_value);

  should_log(42.5, &welfordState, &heartbeatState, &runState, &config);
  ASSERT_DOUBLE_EQUAL(42.5, heartbeatState.last_transmitted_value);

  should_log(9999.0, &welfordState, &heartbeatState, &runState, &config);
  ASSERT_DOUBLE_EQUAL(9999.0, heartbeatState.last_transmitted_value);
}

// ==== regression: sub-hour resolution in the time math ====

void test_backoff_HasSubHourResolution(void) {
  heartbeatState.has_logged = true;
  heartbeatState.last_event_ts = 0;
  double at_1799 = backoff(&heartbeatState, &config, 1799);
  double at_3599 = backoff(&heartbeatState, &config, 3599);
  TEST_ASSERT_TRUE(at_3599 > at_1799);
  TEST_ASSERT_TRUE(at_1799 > (double)config.base_heartbeat_hours);
}

void test_backoff_GrowsWithFractionalDoublingPeriods(void) {
  heartbeatState.has_logged = true;
  heartbeatState.last_event_ts = 0;
  uint32_t now = (config.doubling_hours * 3600) / 2;
  double result = backoff(&heartbeatState, &config, now);
  ASSERT_DOUBLE_WITHIN(1e-6, config.base_heartbeat_hours * sqrt(2.0), result);
}

// ==== startup gating ====

void test_numSamplesInStartup_ComputesFromPeriodAndWindow(void) {
  ALIAUserConfig cfg = config;
  cfg.sample_rate = 300;          // one sample every 5 minutes
  cfg.std_dev_window_hours = 12;  // 12h / 5min == 144 samples
  numSamplesInStartup(&cfg);
  TEST_ASSERT_EQUAL_UINT32(144, cfg.num_startup_samples);
}

void test_numSamplesInStartup_ClampsToWindowCapacity(void) {
  ALIAUserConfig cfg = config;
  cfg.sample_rate = 300;
  cfg.std_dev_window_hours = 240;  // would need 2880 samples
  numSamplesInStartup(&cfg);
  TEST_ASSERT_EQUAL_UINT32(ALIA_STD_DEV_WINDOW_SAMPLES,
                           cfg.num_startup_samples);
}

void test_numSamplesInStartup_ZeroSampleRateDoesNotDivideByZero(void) {
  ALIAUserConfig cfg = config;
  cfg.sample_rate = 0;
  cfg.std_dev_window_hours = 12;
  numSamplesInStartup(&cfg);
  TEST_ASSERT_EQUAL_UINT32(0, cfg.num_startup_samples);
}

void test_alia_startup_complete_TracksSampleCount(void) {
  config.num_startup_samples = 10;
  for (int i = 0; i < 9; i++) {
    welford_push(&welfordState, 1.0);
  }
  TEST_ASSERT_FALSE(alia_startup_complete(&welfordState, &config));
  welford_push(&welfordState, 1.0);
  TEST_ASSERT_TRUE(alia_startup_complete(&welfordState, &config));
}

void test_should_log_DuringStartup_UsesResolutionFloor(void) {
  config.num_startup_samples = 100;
  should_log(0.0, &welfordState, &heartbeatState, &runState, &config);
  for (int i = 0; i < 10; i++) {
    should_log((i % 2) ? 100.0 : -100.0, &welfordState, &heartbeatState,
               &runState, &config);
  }
  TEST_ASSERT_FALSE(alia_startup_complete(&welfordState, &config));

  heartbeatState.last_transmitted_value = 10.0;
  bool result =
      should_log(10.5, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_TRUE(result);
}

void test_should_log_AlwaysUpdatesWindow(void) {
  size_t count_before = welfordState.count;
  should_log(50.0, &welfordState, &heartbeatState, &runState, &config);
  TEST_ASSERT_EQUAL(count_before + 1, welfordState.count);
}

// ==== ALIARegistry: per-stream state isolation ====

void test_alia_registry_init_ReleasesAllSlots(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  for (size_t i = 0; i < ALIA_MAX_STREAMS; i++) {
    TEST_ASSERT_FALSE(reg.streams[i].in_use);
  }
}

void test_alia_stream_get_SameKeyReturnsSameSlot(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  ALIAUserConfig seed = config;

  ALIAStream* first = alia_stream_get(&reg, 10, &seed, 0.1);
  ALIAStream* again = alia_stream_get(&reg, 10, &seed, 0.1);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_EQUAL_PTR(first, again);
}

void test_alia_stream_get_DistinctKeysGetDistinctSlots(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  ALIAUserConfig seed = config;

  ALIAStream* a = alia_stream_get(&reg, 10, &seed, 0.1);
  ALIAStream* b = alia_stream_get(&reg, 9, &seed, 0.1);
  ALIAStream* c = alia_stream_get(&reg, 11, &seed, 0.1);
  TEST_ASSERT_TRUE(a != b && b != c && a != c);
}

void test_alia_stream_get_SeedsResolutionPerStream(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);

  ALIAUserConfig seed = config;

  ALIAStream* temp = alia_stream_get(&reg, 10, &seed, 1.0);
  ALIAStream* pres = alia_stream_get(&reg, 9, &seed, 10.0);

  ASSERT_DOUBLE_EQUAL(1.0, temp->config.sensor_resolution);
  ASSERT_DOUBLE_EQUAL(10.0, pres->config.sensor_resolution);
}

void test_alia_stream_get_ReseedDoesNotDisturbRunningStream(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);

  ALIAUserConfig seed = config;
  ALIAStream* stream = alia_stream_get(&reg, 10, &seed, 1.0);
  should_log(42.0, &stream->welford, &stream->heartbeat, &stream->run,
             &stream->config);
  size_t count_before = stream->welford.count;

  ALIAStream* again = alia_stream_get(&reg, 10, &seed, 999.0);

  TEST_ASSERT_EQUAL_PTR(stream, again);
  ASSERT_DOUBLE_EQUAL(1.0, again->config.sensor_resolution);
  TEST_ASSERT_EQUAL(count_before, again->welford.count);
}

void test_alia_stream_get_AppliesStartupSampleCount(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  ALIAUserConfig seed = config;
  seed.num_startup_samples = 0;

  ALIAStream* stream = alia_stream_get(&reg, 10, &seed, 0.1);
  ALIAUserConfig expected = seed;
  numSamplesInStartup(&expected);
  TEST_ASSERT_EQUAL(expected.num_startup_samples,
                    stream->config.num_startup_samples);
}

void test_alia_stream_get_WhenFullReturnsNullAndKeepsStreams(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  ALIAUserConfig seed = config;

  for (uint32_t k = 0; k < ALIA_MAX_STREAMS; k++) {
    TEST_ASSERT_NOT_NULL(alia_stream_get(&reg, k, &seed, 0.1));
  }

  TEST_ASSERT_NULL(alia_stream_get(&reg, ALIA_MAX_STREAMS, &seed, 0.1));
  for (uint32_t k = 0; k < ALIA_MAX_STREAMS; k++) {
    TEST_ASSERT_TRUE(reg.streams[k].in_use);
    TEST_ASSERT_EQUAL(k, reg.streams[k].key);
  }
}

void test_alia_stream_get_StreamsDoNotShareStatistics(void) {
  static ALIARegistry reg;
  alia_registry_init(&reg);
  ALIAUserConfig seed = config;

  ALIAStream* small = alia_stream_get(&reg, 10, &seed, 0.1);
  ALIAStream* large = alia_stream_get(&reg, 9, &seed, 0.1);

  for (int i = 0; i < 20; i++) {
    welford_push(&small->welford, 22.0 + (i % 2));
    welford_push(&large->welford, 101325.0 + (i % 2));
  }

  ASSERT_DOUBLE_WITHIN(1e-6, 22.5, welford_get_mean(&small->welford));
  ASSERT_DOUBLE_WITHIN(1e-6, 101325.5, welford_get_mean(&large->welford));
  ASSERT_DOUBLE_WITHIN(1e-6, welford_get_stddev(&small->welford),
                       welford_get_stddev(&large->welford));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_welford_init_ZeroState);

  RUN_TEST(test_welford_push_SingleValue_MeanEqualsValue);
  RUN_TEST(test_welford_push_KnownSequence_MatchesNaiveRecompute);
  RUN_TEST(test_welford_push_ConstantValues_VarianceIsZero);

  RUN_TEST(test_welford_push_PastCapacity_MatchesNaiveSlidingWindow);
  RUN_TEST(test_welford_push_HeadWrapsCorrectly);
  RUN_TEST(test_welford_push_ConstantValues_PastCapacity_VarianceStaysZero);

  RUN_TEST(test_welford_get_variance_LessThanTwoSamples_ReturnsZero);
  RUN_TEST(test_welford_get_stddev_MatchesSqrtOfVariance);

  RUN_TEST(test_welford_window_is_full_FalseBeforeCapacity);
  RUN_TEST(test_welford_window_is_full_TrueAtCapacity);

  RUN_TEST(test_backoff_NotYetLogged_ReturnsBaseHeartbeat);
  RUN_TEST(test_backoff_ZeroElapsed_ReturnsBaseHeartbeat);
  RUN_TEST(test_backoff_GrowsWithElapsedTime);
  RUN_TEST(test_backoff_CapsAtMaxHeartbeat);
  RUN_TEST(test_backoff_HasSubHourResolution);
  RUN_TEST(test_backoff_GrowsWithFractionalDoublingPeriods);

  RUN_TEST(test_should_log_FirstCall_ReturnsTrue);
  RUN_TEST(test_should_log_NoEventNoHeartbeat_ReturnsFalse);
  RUN_TEST(test_should_log_LargeDeviation_TriggersEvent);
  RUN_TEST(test_should_log_HeartbeatElapsed_TriggersEvenWithoutDeviation);
  RUN_TEST(test_should_log_AlwaysUpdatesWindow);

  RUN_TEST(test_should_log_HeartbeatDoesNotResetBackoffClock);
  RUN_TEST(test_should_log_EventResetsBackoffClock);
  RUN_TEST(test_should_log_UpdatesLastTransmittedValue);

  RUN_TEST(test_numSamplesInStartup_ComputesFromPeriodAndWindow);
  RUN_TEST(test_numSamplesInStartup_ClampsToWindowCapacity);
  RUN_TEST(test_numSamplesInStartup_ZeroSampleRateDoesNotDivideByZero);
  RUN_TEST(test_alia_startup_complete_TracksSampleCount);
  RUN_TEST(test_should_log_DuringStartup_UsesResolutionFloor);

  RUN_TEST(test_alia_registry_init_ReleasesAllSlots);
  RUN_TEST(test_alia_stream_get_SameKeyReturnsSameSlot);
  RUN_TEST(test_alia_stream_get_DistinctKeysGetDistinctSlots);
  RUN_TEST(test_alia_stream_get_SeedsResolutionPerStream);
  RUN_TEST(test_alia_stream_get_ReseedDoesNotDisturbRunningStream);
  RUN_TEST(test_alia_stream_get_AppliesStartupSampleCount);
  RUN_TEST(test_alia_stream_get_WhenFullReturnsNullAndKeepsStreams);
  RUN_TEST(test_alia_stream_get_StreamsDoNotShareStatistics);

  return UNITY_END();
}
