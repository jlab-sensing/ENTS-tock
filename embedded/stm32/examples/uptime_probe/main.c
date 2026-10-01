/**
 * @file main.c
 * @brief Measures the board facts that the uptime tracking design depends on.
 *
 * Every number this prints is currently an assumption in
 * UPTIME_TRACKING_DESIGN.md. Flash it, reset the board a few times, and the
 * open questions in that document are answered from hardware instead of
 * guessed:
 *
 *   1. Alarm frequency, which sets the tick wrap period and therefore the
 *      minimum heartbeat cadence.
 *   2. FRAM size, and whether reads and writes actually work at the high
 *      addresses the design wants to put the record at.
 *   3. Whether the RTC survives a reset, which decides if downtime can be
 *      measured at all or only detected.
 *   4. That a boot counter in FRAM survives resets and power cycles.
 *
 * Non destructive: the FRAM scratch test saves and restores whatever was at
 * the test addresses. The boot record itself lives in unallocated space well
 * above the measurement FIFO.
 */

#include <libents/storage/fram.h>
#include <libents/util/time.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/peripherals/syscalls/alarm_syscalls.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** Where the boot record lives. Well clear of FRAM_BUFFER_END (1769) so the
 * measurement FIFO can grow without colliding. Two slots for ping pong writes,
 * because a brownout mid write is the exact event being measured. */
#define RECORD_A 0x1F000
#define RECORD_B 0x1F100

/** Scratch addresses for the read/write probe, distinct from the record. */
#define SCRATCH 0x1F200

/** Drives the multi boot self test, separate from the record under test so the
 * test harness state cannot be confused with the thing it is measuring. */
#define PHASE_ADDR 0x1F300
#define PHASE_MAGIC 0x50485345u /* "PHSE" */

/** Stand in epoch for a successful timesync, 2026-01-01. */
#define SIM_EPOCH 1767225600u
/** Simulated outage length between the two halves of the downtime test. */
#define SIM_GAP 7200u

#define RECORD_MAGIC 0x454E5455u /* "ENTU" */
#define RECORD_VERSION 1

/** Mirrors the record in UPTIME_TRACKING_DESIGN.md, trimmed to what the probe
 * needs. Kept to fixed width types so the on disk layout is unambiguous. */
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t seq;
  uint32_t boot_count;
  uint32_t unclean_boots;
  uint32_t last_epoch; /* RTC at the last heartbeat of the previous boot */
  uint32_t last_ticks; /* alarm ticks at that same moment */
  uint32_t checksum;   /* see record_checksum() */
} probe_record_t;

#define FLAG_CLEAN_SHUTDOWN 0x0001u
#define FLAG_TIME_VALID 0x0002u

/**
 * @brief Additive checksum over everything before the checksum field.
 *
 * Deliberately not CRC32: this is a probe, and the point is to detect a torn
 * or never written slot, not to be cryptographically sound. The real record
 * should use CRC32.
 */
static uint32_t record_checksum(const probe_record_t* r) {
  const uint8_t* p = (const uint8_t*)r;
  size_t n = offsetof(probe_record_t, checksum);
  uint32_t sum = 0x9E3779B9u;
  for (size_t i = 0; i < n; i++) {
    sum = (sum << 1) ^ (sum >> 31) ^ p[i];
  }
  return sum;
}

static bool record_valid(const probe_record_t* r) {
  return r->magic == RECORD_MAGIC && r->checksum == record_checksum(r);
}

/**
 * @brief Read both slots and hand back the valid one with the higher seq.
 *
 * @param out Receives the winning record, or a zeroed record if neither slot
 * validates.
 * @param which Receives 'A', 'B', or '-' when neither is usable.
 * @return true if a valid record was found.
 */
static bool record_load(probe_record_t* out, char* which) {
  probe_record_t a = {};
  probe_record_t b = {};

  bool a_ok = fram_read(RECORD_A, sizeof(a), (uint8_t*)&a) == FRAM_OK &&
              record_valid(&a);
  bool b_ok = fram_read(RECORD_B, sizeof(b), (uint8_t*)&b) == FRAM_OK &&
              record_valid(&b);

  *which = '-';
  memset(out, 0, sizeof(*out));

  if (a_ok && b_ok) {
    // Unsigned comparison so a seq wrap still picks the newer one.
    bool a_newer = (uint32_t)(a.seq - b.seq) < 0x80000000u;
    *out = a_newer ? a : b;
    *which = a_newer ? 'A' : 'B';
    return true;
  }
  if (a_ok) {
    *out = a;
    *which = 'A';
    return true;
  }
  if (b_ok) {
    *out = b;
    *which = 'B';
    return true;
  }
  return false;
}

/** Write to whichever slot did not supply the current record. */
static fram_status record_store(probe_record_t* r, char last_used) {
  r->magic = RECORD_MAGIC;
  r->version = RECORD_VERSION;
  r->seq++;
  r->checksum = record_checksum(r);

  fram_addr target = (last_used == 'A') ? RECORD_B : RECORD_A;
  return fram_write(target, (const uint8_t*)r, sizeof(*r));
}

/* ------------------------------------------------------------------------ */
/* Multi boot self test state.                                                */
/*                                                                            */
/* Two branches of the design cannot be checked in a single run, because each */
/* needs the *previous* boot to have set something up: */
/*                                                                            */
/*   - a clean shutdown on boot N must read back as clean on boot N+1 */
/*   - downtime needs a valid clock on both sides of a reset */
/*                                                                            */
/* So the probe walks a small state machine kept in FRAM, advancing one step */
/* per reset. set_epoch() stands in for lorawan_timesync(), which lets the */
/* whole thing run on a bench board with no network and no VBAT. */
/* ------------------------------------------------------------------------ */

typedef struct {
  uint32_t magic;
  uint32_t phase;
  uint32_t checksum;
} phase_rec_t;

static uint32_t phase_checksum(const phase_rec_t* p) {
  return p->magic ^ (p->phase * 2654435761u);
}

static uint32_t phase_load(void) {
  phase_rec_t p = {};
  if (fram_read(PHASE_ADDR, sizeof(p), (uint8_t*)&p) != FRAM_OK) return 0;
  if (p.magic != PHASE_MAGIC || p.checksum != phase_checksum(&p)) return 0;
  return p.phase;
}

static void phase_store(uint32_t phase) {
  phase_rec_t p = {};
  p.magic = PHASE_MAGIC;
  p.phase = phase;
  p.checksum = phase_checksum(&p);
  fram_write(PHASE_ADDR, (const uint8_t*)&p, sizeof(p));
}

/** Running tally so the last phase can print a verdict. */
static int g_checks = 0;
static int g_failures = 0;

static void check(const char* label, unsigned long got, unsigned long want) {
  g_checks++;
  bool ok = (got == want);
  if (!ok) g_failures++;
  printf("  [%s] %s: got %lu, want %lu\n", ok ? "PASS" : "FAIL", label, got,
         want);
}

/** Human readable seconds. */
static void print_duration(const char* label, uint32_t seconds) {
  printf("%s%lu s (%lu h %lu m %lu s)\n", label, (unsigned long)seconds,
         (unsigned long)(seconds / 3600),
         (unsigned long)((seconds % 3600) / 60), (unsigned long)(seconds % 60));
}

/**
 * @brief Prove FRAM reads and writes work at the address the design proposes.
 *
 * Saves the existing bytes and puts them back, so running this cannot damage
 * anything that happens to already live there.
 */
static void probe_fram_scratch(void) {
  const uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF, 0x55, 0xAA};
  uint8_t saved[sizeof(pattern)] = {};
  uint8_t readback[sizeof(pattern)] = {};

  printf("\n-- FRAM read/write at 0x%X --\n", SCRATCH);

  if (fram_read(SCRATCH, sizeof(saved), saved) != FRAM_OK) {
    printf("  FAIL: could not read scratch\n");
    return;
  }
  if (fram_write(SCRATCH, pattern, sizeof(pattern)) != FRAM_OK) {
    printf("  FAIL: could not write scratch\n");
    return;
  }
  if (fram_read(SCRATCH, sizeof(readback), readback) != FRAM_OK) {
    printf("  FAIL: could not read back scratch\n");
    return;
  }

  bool match = memcmp(pattern, readback, sizeof(pattern)) == 0;
  printf("  wrote   ");
  for (size_t i = 0; i < sizeof(pattern); i++) printf("%02X ", pattern[i]);
  printf("\n  read    ");
  for (size_t i = 0; i < sizeof(readback); i++) printf("%02X ", readback[i]);
  printf("\n  %s\n", match ? "PASS" : "FAIL: readback differs");

  // Put back whatever was there.
  fram_write(SCRATCH, saved, sizeof(saved));
}

int main(void) {
  printf("\n==================================================\n");
  printf(" ENTS uptime probe\n");
  printf("==================================================\n");

  //
  // 1. Alarm frequency. This sets the tick wrap period, which sets the
  //    heartbeat cadence the design has to hit.
  //
  uint32_t freq = 0;
  uint32_t ticks_now = 0;
  int rc_freq = libtock_alarm_command_get_frequency(&freq);
  int rc_read = libtock_alarm_command_read(&ticks_now);

  printf("\n-- alarm --\n");
  if (rc_freq != 0 || freq == 0) {
    printf("  FAIL: get_frequency returned %d, freq %lu\n", rc_freq,
           (unsigned long)freq);
  } else {
    printf("  frequency:   %lu Hz\n", (unsigned long)freq);
    printf("  ticks now:   %lu\n", (unsigned long)ticks_now);
    // 2^32 ticks / freq, computed in 64 bit so it cannot overflow.
    uint32_t wrap_s = (uint32_t)(((uint64_t)1 << 32) / freq);
    print_duration("  wrap period: ", wrap_s);
    printf("  => heartbeat must run at least every %lu s\n",
           (unsigned long)(wrap_s / 10));
  }
  if (rc_read != 0) {
    printf("  NOTE: command_read returned %d\n", rc_read);
  }

  //
  // 2. FRAM geometry and whether the proposed address is usable.
  //
  fram_addr size = fram_size();
  printf("\n-- FRAM --\n");
  printf("  size:        %lu bytes\n", (unsigned long)size);
  printf("  record A:    0x%X\n", RECORD_A);
  printf("  record B:    0x%X\n", RECORD_B);
  printf("  record size: %u bytes\n", (unsigned)sizeof(probe_record_t));
  if (RECORD_B + sizeof(probe_record_t) > size) {
    printf("  FAIL: record does not fit, pick lower addresses\n");
  } else {
    printf("  fits with %lu bytes to spare\n",
           (unsigned long)(size - (RECORD_B + sizeof(probe_record_t))));
  }

  probe_fram_scratch();

  //
  // 3. The RTC, read before anything syncs it. If this comes back as a
  //    plausible recent epoch after a power cycle then VBAT is holding and
  //    downtime can actually be measured. If it comes back near zero, downtime
  //    can only be detected, not measured.
  //
  uint32_t epoch_now = epoch();
  printf("\n-- RTC at boot, before any timesync --\n");
  printf("  epoch: %lu\n", (unsigned long)epoch_now);
  // 1735689600 is 2025-01-01. Anything below that on a deployed node means the
  // clock did not survive.
  bool time_valid = epoch_now > 1735689600u;
  printf("  %s\n", time_valid
                       ? "looks valid, RTC survived (VBAT is holding)"
                       : "NOT valid, RTC reset (no VBAT, or first ever boot)");

  //
  // 4. The boot record. This is the mechanism the design rests on, so prove it
  //    survives a reset before building anything on top of it.
  //
  probe_record_t rec = {};
  char slot = '-';
  bool found = record_load(&rec, &slot);

  printf("\n-- boot record --\n");
  if (!found) {
    printf("  no valid record, treating as first ever boot\n");
    memset(&rec, 0, sizeof(rec));
  } else {
    printf("  loaded from slot %c, seq %lu\n", slot, (unsigned long)rec.seq);
    printf("  previous boot_count:    %lu\n", (unsigned long)rec.boot_count);
    printf("  previous unclean_boots: %lu\n", (unsigned long)rec.unclean_boots);

    bool was_clean = (rec.flags & FLAG_CLEAN_SHUTDOWN) != 0;
    printf("  previous shutdown:      %s\n", was_clean ? "clean" : "UNCLEAN");
    if (!was_clean) {
      rec.unclean_boots++;
    }

    // Downtime, but only when both clocks can be trusted. Subtracting a stale
    // epoch from a reset RTC produces garbage, so it is gated.
    if (time_valid && (rec.flags & FLAG_TIME_VALID) && rec.last_epoch != 0) {
      if (epoch_now >= rec.last_epoch) {
        print_duration("  measured downtime:      ",
                       epoch_now - rec.last_epoch);
      } else {
        printf("  measured downtime:      n/a, clock went backwards\n");
      }
    } else {
      printf(
          "  measured downtime:      n/a, need a valid clock on both boots\n");
    }
  }

  bool prev_clean = found && (rec.flags & FLAG_CLEAN_SHUTDOWN) != 0;
  uint32_t prev_epoch = rec.last_epoch;
  bool prev_time_valid = (rec.flags & FLAG_TIME_VALID) != 0;

  rec.boot_count++;
  printf("  this boot is number %lu\n", (unsigned long)rec.boot_count);

  //
  // 5. The multi boot self test. Each phase checks what the previous phase set
  //    up, then arranges the next one. Phase lives in FRAM so it advances one
  //    step per reset.
  //
  uint32_t phase = phase_load();
  bool mark_clean = false;
  // Set when a phase has already decided last_epoch, so the generic boot time
  // update below does not clobber it. Today time_valid is always false on this
  // board so the clobber cannot happen, but that is a property of the bench
  // unit rather than something the test should depend on.
  bool epoch_set_by_phase = false;

  printf("\n-- self test, phase %lu --\n", (unsigned long)phase);

  switch (phase) {
    case 0:
      // Arrange only. Pretend timesync just landed, then exit cleanly, so the
      // next boot has both a stored epoch and a clean shutdown flag to find.
      printf("  arranging: simulating timesync, then a CLEAN shutdown\n");
      set_epoch(SIM_EPOCH);
      printf("  set_epoch(%lu), epoch() now reads %lu\n",
             (unsigned long)SIM_EPOCH, (unsigned long)epoch());
      check("set_epoch took effect", epoch(), SIM_EPOCH);
      rec.flags |= FLAG_TIME_VALID;
      rec.last_epoch = SIM_EPOCH;
      epoch_set_by_phase = true;
      mark_clean = true;
      phase_store(1);
      break;

    case 1:
      // Check the clean shutdown branch, which every run before this one left
      // completely unexercised.
      printf("  checking the CLEAN shutdown branch\n");
      check("previous shutdown was clean", prev_clean ? 1 : 0, 1);
      check("previous epoch survived", prev_epoch, SIM_EPOCH);
      check("previous time_valid survived", prev_time_valid ? 1 : 0, 1);

      // Now the deferred subtraction. The RTC was wiped by the reset, so this
      // is exactly the field case: stale stored epoch, fresh timesync, compute
      // the gap only once the new clock is trustworthy.
      printf("  RTC after reset reads %lu (expected to be stale)\n",
             (unsigned long)epoch_now);
      check("clock was indeed stale at boot", time_valid ? 1 : 0, 0);

      set_epoch(SIM_EPOCH + SIM_GAP);
      printf("  simulated timesync -> epoch() now %lu\n",
             (unsigned long)epoch());
      if (prev_time_valid && epoch() >= prev_epoch) {
        uint32_t downtime = epoch() - prev_epoch;
        print_duration("  measured downtime: ", downtime);
        check("downtime matches the simulated outage", downtime, SIM_GAP);
      } else {
        check("downtime computable", 0, 1);
      }

      // Leave dirty this time so the next boot sees the other branch.
      printf("  arranging: leaving UNCLEAN for the next boot\n");
      rec.flags |= FLAG_TIME_VALID;
      rec.last_epoch = epoch();
      epoch_set_by_phase = true;
      mark_clean = false;
      phase_store(2);
      break;

    case 2:
      printf("  checking the UNCLEAN shutdown branch\n");
      check("previous shutdown was unclean", prev_clean ? 1 : 0, 0);
      printf("  self test complete, wrapping back to phase 0\n");
      phase_store(0);
      break;

    default:
      printf("  unexpected phase, resetting to 0\n");
      phase_store(0);
      break;
  }

  // Record the shutdown disposition. Setting the flag is what a graceful exit
  // would do; leaving it clear is what a brownout leaves behind.
  if (mark_clean) {
    rec.flags |= FLAG_CLEAN_SHUTDOWN;
  } else {
    rec.flags &= (uint16_t)~FLAG_CLEAN_SHUTDOWN;
  }
  if (time_valid && !epoch_set_by_phase) {
    rec.flags |= FLAG_TIME_VALID;
    rec.last_epoch = epoch_now;
  }
  rec.last_ticks = ticks_now;

  fram_status st = record_store(&rec, slot);
  printf("  stored to slot %c as %s: %s\n", (slot == 'A') ? 'B' : 'A',
         mark_clean ? "CLEAN" : "UNCLEAN", st == FRAM_OK ? "OK" : "FAILED");

  if (g_checks > 0) {
    printf("  phase result: %d/%d checks passed\n", g_checks - g_failures,
           g_checks);
  }

  //
  // 5. Watch ticks advance, so the accumulate-across-wrap math can be eyeballed
  //    against a real clock rather than trusted on paper.
  //
  printf("\n-- tick accumulation, 5 samples 2 s apart --\n");
  uint32_t last = 0;
  uint64_t accum = 0;
  libtock_alarm_command_read(&last);
  for (int i = 0; i < 5; i++) {
    libtocksync_alarm_delay_ms(2000);

    uint32_t now = 0;
    libtock_alarm_command_read(&now);

    // Unsigned subtraction is correct across a single wrap, which is the whole
    // trick the heartbeat relies on.
    uint32_t delta = now - last;
    last = now;
    accum += delta;

    printf("  sample %d: ticks %10lu  delta %8lu", i + 1, (unsigned long)now,
           (unsigned long)delta);
    if (freq > 0) {
      printf("  (%lu ms)", (unsigned long)((uint64_t)delta * 1000 / freq));
    }
    printf("\n");
  }
  if (freq > 0) {
    printf("  accumulated %lu ticks = %lu ms over ~10000 ms of delays\n",
           (unsigned long)accum, (unsigned long)(accum * 1000 / freq));
  }

  printf("\n==================================================\n");
  printf(" Reset the board to advance the self test. It runs\n");
  printf(" over three boots:\n");
  printf("   phase 0  arrange a clean shutdown\n");
  printf("   phase 1  check clean was seen, then measure\n");
  printf("            downtime across the reset\n");
  printf("   phase 2  check unclean was seen\n");
  printf("==================================================\n\n");

  return 0;
}
