/**
 * @file main.c
 * @brief Host unit tests for libents/util/uptime.c.
 *
 * Links the real uptime.c against stubbed FRAM, RTC and alarm drivers, so the
 * logic can be exercised on a workstation without a board. The point is the
 * cases that are slow or impossible to provoke on hardware: a tick counter
 * about to wrap, a CRC corrupted slot, a torn write, a clock that steps
 * backwards.
 *
 * A "reboot" here is just another call to ents_uptime_init(). That is faithful
 * to the real thing, because every piece of state the library keeps across a
 * reset lives in the FRAM array below, and init() resets everything else.
 *
 * Build and run: ./run.sh
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../src/libents/storage/fram.h"
#include "../../src/libents/util/uptime.h"

/* -------------------------------------------------------------------------- */
/* Test harness                                                               */
/* -------------------------------------------------------------------------- */

static int checks_run = 0;
static int checks_failed = 0;
static const char* current_group = "";

static void group(const char* name) {
  current_group = name;
  printf("\n-- %s --\n", name);
}

static void check(bool ok, const char* what) {
  checks_run++;
  if (ok) {
    printf("  [PASS] %s\n", what);
  } else {
    checks_failed++;
    printf("  [FAIL] %s  (in %s)\n", what, current_group);
  }
}

static void check_u32(uint32_t got, uint32_t want, const char* what) {
  checks_run++;
  if (got == want) {
    printf("  [PASS] %s: %lu\n", what, (unsigned long)got);
  } else {
    checks_failed++;
    printf("  [FAIL] %s: got %lu, want %lu  (in %s)\n", what,
           (unsigned long)got, (unsigned long)want, current_group);
  }
}

/* -------------------------------------------------------------------------- */
/* Stubbed FRAM                                                               */
/* -------------------------------------------------------------------------- */

/** MB85RC1MT size, so the address arithmetic under test is the real one. */
#define FRAM_SIZE 131072u

static uint8_t g_fram[FRAM_SIZE];
static int g_fram_writes = 0;
static bool g_fram_fail = false;

fram_status fram_write(fram_addr addr, const uint8_t* data, size_t len) {
  if (g_fram_fail) {
    return FRAM_ERROR;
  }
  if (addr + len > FRAM_SIZE) {
    return FRAM_OUT_OF_RANGE;
  }
  memcpy(&g_fram[addr], data, len);
  g_fram_writes++;
  return FRAM_OK;
}

fram_status fram_read(fram_addr addr, size_t len, uint8_t* data) {
  if (g_fram_fail) {
    return FRAM_ERROR;
  }
  if (addr + len > FRAM_SIZE) {
    return FRAM_OUT_OF_RANGE;
  }
  memcpy(data, &g_fram[addr], len);
  return FRAM_OK;
}

fram_addr fram_size(void) { return FRAM_SIZE; }

/* -------------------------------------------------------------------------- */
/* Stubbed RTC                                                                */
/* -------------------------------------------------------------------------- */

/** What the stm32 RTC actually reads after a reset on the bench board. */
#define STALE_EPOCH 946684800u

static uint32_t g_epoch = STALE_EPOCH;

uint32_t epoch(void) { return g_epoch; }
void set_epoch(uint32_t e) { g_epoch = e; }

/* -------------------------------------------------------------------------- */
/* Stubbed alarm                                                              */
/* -------------------------------------------------------------------------- */

/** Measured on the bench stm32wle5jc. Not the 32768 the design first assumed.
 */
#define TICK_HZ 16000u

static uint32_t g_freq = TICK_HZ;
static uint32_t g_ticks = 0;
static bool g_clock_fail = false;

int libtock_alarm_command_get_frequency(uint32_t* frequency) {
  if (g_clock_fail) {
    return -1;
  }
  *frequency = g_freq;
  return 0;
}

int libtock_alarm_command_read(uint32_t* time) {
  if (g_clock_fail) {
    return -1;
  }
  *time = g_ticks;
  return 0;
}

/** Advance the simulated tick counter, wrapping like the real 32 bit one. */
static void advance_ticks(uint32_t ticks) { g_ticks += ticks; }

/** Advance by whole seconds at the configured frequency. */
static void advance_seconds(uint32_t s) { advance_ticks(s * TICK_HZ); }

/* -------------------------------------------------------------------------- */
/* Record layout, mirrored so tests can inspect raw FRAM                      */
/* -------------------------------------------------------------------------- */

#define REC_A 0x1F000
#define REC_B 0x1F100
#define OFF_MAGIC 0
#define OFF_FLAGS 6
#define OFF_SEQ 8
#define OFF_BOOT_COUNT 12
#define RECORD_BYTES 44

static uint32_t raw_u32(fram_addr slot, size_t off) {
  uint32_t v;
  memcpy(&v, &g_fram[slot + off], sizeof(v));
  return v;
}

static bool slot_live(fram_addr slot) {
  return raw_u32(slot, OFF_MAGIC) == 0x454E5455u;
}

/** Wipe the simulated part, as if this node had never run before. */
static void fram_blank(void) {
  memset(g_fram, 0, sizeof(g_fram));
  g_fram_writes = 0;
}

/** Simulate a power cut: FRAM survives, the RTC does not, ticks restart. */
static void power_cycle(void) {
  g_epoch = STALE_EPOCH;
  g_ticks = 0;
}

/* -------------------------------------------------------------------------- */
/* Tests                                                                      */
/* -------------------------------------------------------------------------- */

/**
 * Every entry point has to refuse to run before init(), because the counters
 * are static and would otherwise report a plausible looking zero.
 */
static void test_pre_init_guards(void) {
  group("pre-init guards");
  fram_blank();
  ents_uptime_erase(); /* also clears the init flag */

  check(ents_uptime_accumulate() == ENTS_UPTIME_ERR_NOT_INIT,
        "accumulate() refuses before init");
  check(ents_uptime_tick() == ENTS_UPTIME_ERR_NOT_INIT,
        "tick() refuses before init");
  check(ents_uptime_time_synced() == ENTS_UPTIME_ERR_NOT_INIT,
        "time_synced() refuses before init");
  check(ents_uptime_mark_clean() == ENTS_UPTIME_ERR_NOT_INIT,
        "mark_clean() refuses before init");
  check(ents_uptime_flush() == ENTS_UPTIME_ERR_NOT_INIT,
        "flush() refuses before init");

  ents_uptime_stats s;
  memset(&s, 0xAA, sizeof(s));
  check(ents_uptime_get(&s) == ENTS_UPTIME_ERR_NOT_INIT,
        "get() refuses before init");
  check(s.boot_count == 0, "get() zeroes the output when it refuses");
  check(ents_uptime_get(NULL) == ENTS_UPTIME_ERR_NOT_INIT,
        "get(NULL) is rejected");
}

/** A part that has never held a record has to look like boot 1, not garbage. */
static void test_first_boot(void) {
  group("first ever boot");
  fram_blank();
  power_cycle();

  check(ents_uptime_init() == ENTS_UPTIME_OK, "init() on a blank part");

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.boot_count, 1, "boot_count");
  check_u32(s.unclean_boots, 0, "unclean_boots");
  check_u32(s.session_seconds, 0, "session_seconds");
  check_u32(s.cumulative_seconds, 0, "cumulative_seconds");
  check_u32(s.downtime_seconds, 0, "downtime_seconds");
  check(!s.time_valid, "clock is not trusted before a timesync");
  check(slot_live(REC_A), "the first record lands in slot A");
}

/**
 * The counter that actually matters. Anything that resets without calling
 * mark_clean() has to come back as unclean, because that is the only signal a
 * brownout leaves behind.
 */
static void test_boot_counting(void) {
  group("boot counting and shutdown disposition");
  fram_blank();
  power_cycle();
  ents_uptime_init();

  /* Reset without marking clean: a brownout or a watchdog. */
  power_cycle();
  ents_uptime_init();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.boot_count, 2, "boot_count after a second boot");
  check_u32(s.unclean_boots, 1, "unclean_boots after an unclean reset");
  check(!s.previous_clean, "previous shutdown reported unclean");

  /* Now a graceful shutdown, which must not raise the unclean count. */
  ents_uptime_mark_clean();
  power_cycle();
  ents_uptime_init();

  ents_uptime_get(&s);
  check_u32(s.boot_count, 3, "boot_count after a clean restart");
  check_u32(s.unclean_boots, 1, "unclean_boots unchanged by a clean shutdown");
  check(s.previous_clean, "previous shutdown reported clean");
}

/** Session time is per boot, cumulative time is not. */
static void test_session_vs_cumulative(void) {
  group("session resets, cumulative carries");
  fram_blank();
  power_cycle();
  ents_uptime_init();

  advance_seconds(100);
  ents_uptime_accumulate();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 100, "session after 100 s");
  check_u32(s.cumulative_seconds, 100, "cumulative after 100 s");

  ents_uptime_flush();
  power_cycle();
  ents_uptime_init();
  advance_seconds(40);
  ents_uptime_accumulate();

  ents_uptime_get(&s);
  check_u32(s.session_seconds, 40, "session restarts at the new boot");
  check_u32(s.cumulative_seconds, 140, "cumulative carries across the boot");
}

/**
 * The tick counter is 32 bits. Unsigned subtraction handles the rollover, but
 * only if it is sampled at least once per wrap period, which is the whole
 * reason ents_uptime_accumulate() exists separately from tick().
 */
static void test_tick_wrap(void) {
  group("tick counter wrap");
  fram_blank();
  power_cycle();

  /* Start 100 ticks short of the rollover. */
  g_ticks = 0xFFFFFFFFu - 99u;
  ents_uptime_init();

  /* Cross the boundary by a whole second's worth of ticks. */
  advance_ticks(TICK_HZ);
  ents_uptime_accumulate();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 1, "one second measured across the wrap");
  check(g_ticks < TICK_HZ, "the counter really did wrap");

  /* And a full wrap plus change still lands right. */
  advance_ticks(TICK_HZ * 5);
  ents_uptime_accumulate();
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 6, "further seconds after the wrap");
}

/** Sub second calls must carry the remainder or the clock loses time. */
static void test_subsecond_accumulation(void) {
  group("sub-second accumulation");
  fram_blank();
  power_cycle();
  ents_uptime_init();

  /* Four quarter seconds. Truncating each one would report zero. */
  for (int i = 0; i < 4; i++) {
    advance_ticks(TICK_HZ / 4);
    ents_uptime_accumulate();
  }

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 1, "four quarter seconds make one second");

  /* And the leftover is not double counted on the next whole second. */
  advance_ticks(TICK_HZ / 2);
  ents_uptime_accumulate();
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 1, "half a second does not round up");
  advance_ticks(TICK_HZ / 2);
  ents_uptime_accumulate();
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 2, "the two halves complete a second");
}

/**
 * tick() is meant to be cheap to call often, touching FRAM only on the persist
 * interval. If that throttle broke, the i2c traffic would go up by orders of
 * magnitude and nothing would visibly fail.
 */
static void test_persist_interval(void) {
  group("persist interval throttles FRAM writes");
  fram_blank();
  power_cycle();
  ents_uptime_init();

  int writes_after_init = g_fram_writes;

  /* One second short of the interval: still no write. */
  advance_seconds(ENTS_UPTIME_PERSIST_INTERVAL_S - 1);
  ents_uptime_tick();
  check_u32((uint32_t)(g_fram_writes - writes_after_init), 0,
            "no FRAM write before the interval elapses");

  /* Crossing it writes exactly once. */
  advance_seconds(1);
  ents_uptime_tick();
  check_u32((uint32_t)(g_fram_writes - writes_after_init), 1,
            "one FRAM write when the interval elapses");

  /* Many calls inside the next interval stay free. */
  for (int i = 0; i < 50; i++) {
    advance_seconds(1);
    ents_uptime_tick();
  }
  check_u32((uint32_t)(g_fram_writes - writes_after_init), 1,
            "50 more calls inside the interval cost nothing");
}

/** Writes alternate slots so a torn write cannot destroy the only copy. */
static void test_ping_pong(void) {
  group("A/B ping-pong");
  fram_blank();
  power_cycle();
  ents_uptime_init();

  check(slot_live(REC_A) && !slot_live(REC_B), "boot 1 wrote A only");
  uint32_t seq_a = raw_u32(REC_A, OFF_SEQ);

  power_cycle();
  ents_uptime_init();
  check(slot_live(REC_B), "boot 2 wrote B");
  check(raw_u32(REC_B, OFF_SEQ) > seq_a, "B carries the higher seq");

  power_cycle();
  ents_uptime_init();
  check(raw_u32(REC_A, OFF_SEQ) > raw_u32(REC_B, OFF_SEQ),
        "boot 3 went back to A with a higher seq");
  check_u32(raw_u32(REC_A, OFF_BOOT_COUNT), 3, "the newer slot has boot 3");
}

/** A slot that fails its CRC must be ignored in favour of the older good one.
 */
static void test_crc_rejects_corruption(void) {
  group("CRC rejects a corrupt slot");
  fram_blank();
  power_cycle();
  ents_uptime_init(); /* writes A */
  power_cycle();
  ents_uptime_init(); /* writes B, now the newer */

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.boot_count, 2, "two boots recorded");

  /* Corrupt the newer slot without touching its magic, so only the CRC
   * catches it. */
  g_fram[REC_B + OFF_BOOT_COUNT] ^= 0xFFu;

  power_cycle();
  ents_uptime_init();
  ents_uptime_get(&s);
  /* The stale A said boot_count 1, so this boot is 2 again rather than the
   * garbage value the corrupted slot would have produced. */
  check_u32(s.boot_count, 2, "fell back to the older intact slot");
}

/** A write interrupted by a brownout leaves a half written slot. */
static void test_torn_write(void) {
  group("torn write during a brownout");
  fram_blank();
  power_cycle();
  ents_uptime_init();
  power_cycle();
  ents_uptime_init(); /* A and B both valid now */

  /* Simulate power lost partway through rewriting B: the first half is the new
   * record, the rest is still the old bytes. */
  memset(&g_fram[REC_B + 16], 0x5A, RECORD_BYTES - 16);

  power_cycle();
  check(ents_uptime_init() == ENTS_UPTIME_OK, "init survives a torn slot");

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check(s.boot_count >= 2, "counters came from the intact slot");
}

/** Both slots gone means a fresh start, not a hang or a garbage count. */
static void test_both_slots_invalid(void) {
  group("both slots unreadable");
  fram_blank();
  power_cycle();
  ents_uptime_init();
  power_cycle();
  ents_uptime_init();

  memset(&g_fram[REC_A], 0xFF, RECORD_BYTES);
  memset(&g_fram[REC_B], 0xFF, RECORD_BYTES);

  power_cycle();
  check(ents_uptime_init() == ENTS_UPTIME_OK, "init() recovers");
  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.boot_count, 1, "treated as a first ever boot");
}

/**
 * The core of the whole design. The RTC is wiped by the reset, so the outage
 * can only be measured after the new boot has synced its clock, by subtracting
 * the heartbeat FRAM carried across.
 */
static void test_downtime(void) {
  group("downtime across a power cut");
  fram_blank();
  power_cycle();

  /* Boot 1: comes up, syncs, runs for an hour. */
  ents_uptime_init();
  g_epoch = 1767225600u; /* 2026-01-01 00:00:00 UTC */
  ents_uptime_time_synced();
  advance_seconds(3600);
  g_epoch += 3600u;
  ents_uptime_flush();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check(s.time_valid, "clock trusted after timesync");
  check_u32(s.downtime_seconds, 0, "no downtime yet");

  /* Two hours off. */
  power_cycle();
  ents_uptime_init();
  ents_uptime_get(&s);
  check_u32(g_epoch, STALE_EPOCH, "the RTC really is stale at boot");
  check(!s.time_valid, "clock not trusted at boot");
  check_u32(s.downtime_seconds, 0, "downtime is not computed at boot");

  /* Timesync lands, and only now is the subtraction possible. */
  g_epoch = 1767225600u + 3600u + 7200u;
  ents_uptime_time_synced();
  ents_uptime_get(&s);
  check_u32(s.downtime_seconds, 7200, "two hour outage measured exactly");

  /* A second outage accumulates rather than replacing the first. */
  advance_seconds(60);
  g_epoch += 60u;
  ents_uptime_flush();
  power_cycle();
  ents_uptime_init();
  g_epoch = 1767225600u + 3600u + 7200u + 60u + 1800u;
  ents_uptime_time_synced();
  ents_uptime_get(&s);
  check_u32(s.downtime_seconds, 9000, "outages sum across boots");
}

/**
 * If the previous boot never reached a timesync there is nothing to subtract
 * from, and the honest answer is "restarted, duration unknown" rather than a
 * number derived from the 2000-01-01 default.
 */
static void test_downtime_unmeasurable(void) {
  group("downtime when the previous boot never synced");
  fram_blank();
  power_cycle();

  ents_uptime_init(); /* never calls time_synced */
  advance_seconds(600);
  ents_uptime_flush();

  power_cycle();
  ents_uptime_init();
  g_epoch = 1767225600u;
  ents_uptime_time_synced();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.downtime_seconds, 0, "no bogus downtime invented");
  check_u32(s.boot_count, 2, "the restart is still counted");
  check_u32(s.unclean_boots, 1, "and still flagged unclean");
}

/**
 * timesync can step the RTC in either direction. A backwards step must not
 * underflow downtime into billions of seconds.
 */
static void test_clock_moves_backwards(void) {
  group("clock stepping backwards");
  fram_blank();
  power_cycle();

  ents_uptime_init();
  g_epoch = 1767225600u;
  ents_uptime_time_synced();
  ents_uptime_flush();

  power_cycle();
  ents_uptime_init();
  /* The new sync produces an earlier time than the stored heartbeat. */
  g_epoch = 1767225600u - 5000u;
  ents_uptime_time_synced();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.downtime_seconds, 0, "negative interval ignored, not wrapped");
}

/** Session uptime must come from ticks, never from an epoch difference. */
static void test_timesync_does_not_disturb_session(void) {
  group("a mid-session RTC step does not corrupt uptime");
  fram_blank();
  power_cycle();

  ents_uptime_init();
  advance_seconds(50);
  ents_uptime_accumulate();

  /* A wild step forwards, as a first successful timesync can produce. */
  g_epoch = 1767225600u + 999999u;
  ents_uptime_time_synced();

  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.session_seconds, 50, "session still measured from ticks");
}

/** A driver that will not report its frequency has to fail loudly. */
static void test_no_clock(void) {
  group("alarm driver unavailable");
  fram_blank();
  power_cycle();
  ents_uptime_erase();

  g_clock_fail = true;
  check(ents_uptime_init() == ENTS_UPTIME_ERR_NO_CLOCK,
        "init() reports the missing clock");
  g_clock_fail = false;

  check(ents_uptime_tick() == ENTS_UPTIME_ERR_NOT_INIT,
        "and leaves the library uninitialised");
}

/** A dead FRAM must surface as an error rather than silent zero counters. */
static void test_fram_failure(void) {
  group("FRAM failure");
  fram_blank();
  power_cycle();

  g_fram_fail = true;
  check(ents_uptime_init() == ENTS_UPTIME_ERR_FRAM,
        "init() reports a failed store");
  g_fram_fail = false;
}

/** Provisioning a node has to leave it looking factory fresh. */
static void test_erase(void) {
  group("erase");
  fram_blank();
  power_cycle();
  ents_uptime_init();
  advance_seconds(10);
  ents_uptime_flush();

  check(ents_uptime_erase() == ENTS_UPTIME_OK, "erase() succeeds");
  check(!slot_live(REC_A) && !slot_live(REC_B), "both slots wiped");

  power_cycle();
  ents_uptime_init();
  ents_uptime_stats s;
  ents_uptime_get(&s);
  check_u32(s.boot_count, 1, "next boot looks like the first");
}

/** The record must stay clear of the measurement FIFO at the bottom of FRAM. */
static void test_layout(void) {
  group("FRAM layout");
  check(ENTS_UPTIME_ADDR_A >= 0x1F000, "slot A is high in the part");
  check(ENTS_UPTIME_ADDR_B - ENTS_UPTIME_ADDR_A >= RECORD_BYTES,
        "the slots do not overlap");
  check(ENTS_UPTIME_ADDR_B + RECORD_BYTES <= FRAM_SIZE,
        "slot B fits inside the part");
  check(ENTS_UPTIME_ADDR_A > 1769, "both slots clear the measurement FIFO");
}

/* -------------------------------------------------------------------------- */

int main(void) {
  printf("uptime host unit tests\n");
  printf("alarm %lu Hz, FRAM %lu bytes, persist interval %d s\n",
         (unsigned long)TICK_HZ, (unsigned long)FRAM_SIZE,
         ENTS_UPTIME_PERSIST_INTERVAL_S);

  test_pre_init_guards();
  test_first_boot();
  test_boot_counting();
  test_session_vs_cumulative();
  test_tick_wrap();
  test_subsecond_accumulation();
  test_persist_interval();
  test_ping_pong();
  test_crc_rejects_corruption();
  test_torn_write();
  test_both_slots_invalid();
  test_downtime();
  test_downtime_unmeasurable();
  test_clock_moves_backwards();
  test_timesync_does_not_disturb_session();
  test_no_clock();
  test_fram_failure();
  test_erase();
  test_layout();

  printf("\n%d checks, %d failed\n", checks_run, checks_failed);
  return checks_failed == 0 ? 0 : 1;
}
