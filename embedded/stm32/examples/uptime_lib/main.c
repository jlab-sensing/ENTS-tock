/**
 * @file main.c
 * @brief Exercises the real libents/util/uptime API on hardware.
 *
 * uptime_probe measured the board and validated a hand written record.
 * This runs the actual shipping library against the real FRAM, RTC and alarm,
 * so anything the host unit tests cannot see is caught here: struct padding,
 * FRAM addressing, CRC over real bytes, and the libtock alarm behaviour.
 *
 * Walks a two phase state machine held in FRAM, one step per reset, using
 * set_epoch() to stand in for lorawan_timesync().
 *
 *   phase 0  sync the clock, run a while, shut down CLEAN
 *   phase 1  expect clean, then measure the outage across the reset
 *
 * Reset the board to advance.
 */

#include <libents/storage/fram.h>
#include <libents/util/time.h>
#include <libents/util/uptime.h>
#include <libtock-sync/services/alarm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** Phase state, kept clear of the library's own slots at 0x1F000 / 0x1F100. */
#define PHASE_ADDR 0x1F400
#define PHASE_MAGIC 0x554C4942u /* "ULIB" */

/** Stand in for a timesync, 2026-01-01. */
#define SIM_EPOCH 1767225600u
/** Simulated outage between phase 0 and phase 1. */
#define SIM_GAP 3600u

typedef struct {
  uint32_t magic;
  uint32_t phase;
  uint32_t last_epoch;
  uint32_t checksum;
} phase_rec_t;

static uint32_t phase_sum(const phase_rec_t* p) {
  return p->magic ^ (p->phase * 2654435761u) ^ (p->last_epoch * 40503u);
}

static bool phase_load(phase_rec_t* out) {
  if (fram_read(PHASE_ADDR, sizeof(*out), (uint8_t*)out) != FRAM_OK) {
    return false;
  }
  return out->magic == PHASE_MAGIC && out->checksum == phase_sum(out);
}

static void phase_store(uint32_t phase, uint32_t last_epoch) {
  phase_rec_t p = {};
  p.magic = PHASE_MAGIC;
  p.phase = phase;
  p.last_epoch = last_epoch;
  p.checksum = phase_sum(&p);
  fram_write(PHASE_ADDR, (const uint8_t*)&p, sizeof(p));
}

static int g_checks = 0;
static int g_fails = 0;

static void check(const char* label, unsigned long got, unsigned long want) {
  g_checks++;
  bool ok = (got == want);
  if (!ok) g_fails++;
  printf("  [%s] %s: got %lu, want %lu\n", ok ? "PASS" : "FAIL", label, got,
         want);
}

static void dump(const ents_uptime_stats* s) {
  printf(
      "  boot_count=%lu unclean=%lu session=%lu cumulative=%lu downtime=%lu\n",
      (unsigned long)s->boot_count, (unsigned long)s->unclean_boots,
      (unsigned long)s->session_seconds, (unsigned long)s->cumulative_seconds,
      (unsigned long)s->downtime_seconds);
  printf("  time_valid=%d previous_clean=%d\n", (int)s->time_valid,
         (int)s->previous_clean);
}

int main(void) {
  printf("\n==================================================\n");
  printf(" ENTS uptime library test (real libents API)\n");
  printf("==================================================\n");

  ents_uptime_status rc = ents_uptime_init();
  printf("\nents_uptime_init -> %d\n", (int)rc);
  check("init succeeded", rc == ENTS_UPTIME_OK, 1);
  if (rc != ENTS_UPTIME_OK) {
    printf("cannot continue without init\n");
    return 1;
  }

  ents_uptime_stats st = {};
  ents_uptime_get(&st);
  dump(&st);
  check("boot_count is nonzero", st.boot_count > 0, 1);
  check("time_valid false before sync", st.time_valid, 0);

  phase_rec_t ph = {};
  bool have_phase = phase_load(&ph);
  uint32_t phase = have_phase ? ph.phase : 0;

  printf("\n-- phase %lu --\n", (unsigned long)phase);

  if (phase == 0) {
    printf("  arranging: timesync, run, then a CLEAN shutdown\n");

    set_epoch(SIM_EPOCH);
    rc = ents_uptime_time_synced();
    check("time_synced ok", rc == ENTS_UPTIME_OK, 1);

    ents_uptime_get(&st);
    check("time_valid now true", st.time_valid, 1);

    // Let real seconds pass so session time is genuinely measured by the
    // hardware alarm rather than simulated.
    printf("  running for 3 s of real time...\n");
    libtocksync_alarm_delay_ms(3000);

    ents_uptime_get(&st);
    dump(&st);
    check("session advanced to about 3 s", st.session_seconds >= 2, 1);

    // Advance the RTC so the next boot sees a gap, then exit cleanly.
    set_epoch(SIM_EPOCH + SIM_GAP);
    rc = ents_uptime_mark_clean();
    check("mark_clean ok", rc == ENTS_UPTIME_OK, 1);

    phase_store(1, SIM_EPOCH + SIM_GAP);
    printf("  stored heartbeat at %lu, reset to continue\n",
           (unsigned long)(SIM_EPOCH + SIM_GAP));

  } else {
    printf("  checking what phase 0 left behind\n");

    check("previous shutdown was clean", st.previous_clean, 1);
    check("cumulative survived the reset", st.cumulative_seconds >= 2, 1);

    printf("  RTC at boot reads %lu\n", (unsigned long)epoch());

    // downtime_seconds is cumulative across every outage the node has ever
    // measured, so compare the increase rather than the absolute value.
    uint32_t downtime_before = st.downtime_seconds;

    // Come back 2 h after the heartbeat phase 0 stored.
    uint32_t resume = ph.last_epoch + 7200u;
    set_epoch(resume);
    printf("  simulated timesync -> %lu\n", (unsigned long)resume);

    rc = ents_uptime_time_synced();
    check("time_synced ok", rc == ENTS_UPTIME_OK, 1);

    ents_uptime_get(&st);
    dump(&st);
    printf("  downtime %lu -> %lu\n", (unsigned long)downtime_before,
           (unsigned long)st.downtime_seconds);
    check("this outage added 2 h", st.downtime_seconds - downtime_before, 7200);

    phase_store(0, 0);
    printf("  cycle complete, wrapping back to phase 0\n");
  }

  printf("\n-- result --\n");
  printf("  %d checks, %d failed\n", g_checks, g_fails);
  printf("  %s\n", g_fails ? "FAIL" : "all checks PASS");
  printf("==================================================\n\n");
  return g_fails ? 1 : 0;
}
