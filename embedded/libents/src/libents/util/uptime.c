#include "uptime.h"

#include <libtock/peripherals/syscalls/alarm_syscalls.h>
#include <string.h>

#include "../storage/fram.h"
#include "time.h"

/** Identifies a record slot. "ENTU". */
#define RECORD_MAGIC 0x454E5455u
#define RECORD_VERSION 1

/** Set while the node is running normally, cleared on a graceful shutdown.
 * Inverted sense on purpose: a brownout cannot run code, so the only way the
 * flag ends up clear is if something deliberately cleared it. */
#define FLAG_DIRTY 0x0001u
/**
 * Set once any boot has written a real heartbeat_epoch.
 *
 * This is a property of the stored record, NOT of the current session, and it
 * must therefore survive a reset. Whether *this* boot has a trustworthy clock
 * is tracked separately in s_time_synced, which is RAM only and starts false.
 *
 * Collapsing the two into one flag looks harmless and is not: clearing it at
 * init throws away the only evidence that the previous boot's heartbeat meant
 * anything, and downtime silently becomes unmeasurable forever.
 */
#define FLAG_HEARTBEAT_VALID 0x0002u

/**
 * On disk layout. Fixed width fields only, and no implicit padding, so the
 * bytes mean the same thing to every build.
 */
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t seq;
  uint32_t boot_count;
  uint32_t unclean_boots;
  uint32_t session_seconds;
  uint32_t cumulative_seconds;
  uint32_t boot_epoch;
  uint32_t heartbeat_epoch;
  uint32_t downtime_seconds;
  uint32_t crc32;
} record_t;

static record_t s_rec;
/** Which slot the live record came from, so writes go to the other one. */
static fram_addr s_last_slot;
static bool s_init;
/** Previous shutdown disposition, kept for reporting after init overwrites it.
 */
static bool s_prev_clean;
/** True once THIS boot has reported a timesync. RAM only, so it starts false
 * on every boot regardless of what the stored record says. */
static bool s_time_synced;

/** Alarm frequency in ticks per second, read once at init. */
static uint32_t s_freq;
/** Tick counter at the last accumulate. */
static uint32_t s_last_ticks;
/** Ticks left over from the last whole second, so the count does not drift. */
static uint32_t s_tick_rem;
/** session_seconds at the last FRAM write, drives the persist interval. */
static uint32_t s_last_persist_s;

/* -------------------------------------------------------------------------- */

/**
 * @brief Bitwise CRC32, the standard reflected polynomial.
 *
 * No lookup table. This runs a few times a boot at most, so 1 KB of table would
 * be a poor trade on a part this size.
 */
static uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

/** CRC over everything ahead of the crc32 field itself. */
static uint32_t record_crc(const record_t* r) {
  return crc32((const uint8_t*)r, offsetof(record_t, crc32));
}

static bool record_valid(const record_t* r) {
  return r->magic == RECORD_MAGIC && r->crc32 == record_crc(r);
}

/**
 * @brief Read both slots and pick the valid one with the higher seq.
 *
 * @param out Receives the winning record, untouched if none is valid.
 * @param slot Receives the address it came from.
 * @return true if a valid record was found.
 */
static bool record_load(record_t* out, fram_addr* slot) {
  record_t a;
  record_t b;
  bool a_ok =
      fram_read(ENTS_UPTIME_ADDR_A, sizeof(a), (uint8_t*)&a) == FRAM_OK &&
      record_valid(&a);
  bool b_ok =
      fram_read(ENTS_UPTIME_ADDR_B, sizeof(b), (uint8_t*)&b) == FRAM_OK &&
      record_valid(&b);

  if (a_ok && b_ok) {
    // Unsigned wraparound compare, so a seq that rolls over still orders right.
    bool a_newer = (uint32_t)(a.seq - b.seq) < 0x80000000u;
    *out = a_newer ? a : b;
    *slot = a_newer ? ENTS_UPTIME_ADDR_A : ENTS_UPTIME_ADDR_B;
    return true;
  }
  if (a_ok) {
    *out = a;
    *slot = ENTS_UPTIME_ADDR_A;
    return true;
  }
  if (b_ok) {
    *out = b;
    *slot = ENTS_UPTIME_ADDR_B;
    return true;
  }
  return false;
}

/** Write the live record to whichever slot it did not come from. */
static ents_uptime_status record_store(void) {
  fram_addr target = (s_last_slot == ENTS_UPTIME_ADDR_A) ? ENTS_UPTIME_ADDR_B
                                                         : ENTS_UPTIME_ADDR_A;
  s_rec.magic = RECORD_MAGIC;
  s_rec.version = RECORD_VERSION;
  s_rec.seq++;
  s_rec.crc32 = record_crc(&s_rec);

  if (fram_write(target, (const uint8_t*)&s_rec, sizeof(s_rec)) != FRAM_OK) {
    return ENTS_UPTIME_ERR_FRAM;
  }
  s_last_slot = target;
  s_last_persist_s = s_rec.session_seconds;
  return ENTS_UPTIME_OK;
}

/**
 * @brief Fold elapsed ticks into the second counters.
 *
 * Unsigned subtraction is correct across a single wrap of the 32 bit counter,
 * which is why this must be called more often than the wrap period. The
 * remainder is carried so repeated sub second calls do not lose time.
 */
static void accumulate(void) {
  if (s_freq == 0) {
    return;
  }

  uint32_t now = 0;
  if (libtock_alarm_command_read(&now) != 0) {
    return;
  }

  uint32_t delta = now - s_last_ticks;
  s_last_ticks = now;

  uint32_t total = delta + s_tick_rem;
  uint32_t secs = total / s_freq;
  s_tick_rem = total % s_freq;

  s_rec.session_seconds += secs;
  s_rec.cumulative_seconds += secs;
}

/* -------------------------------------------------------------------------- */

ents_uptime_status ents_uptime_init(void) {
  memset(&s_rec, 0, sizeof(s_rec));
  s_last_slot = ENTS_UPTIME_ADDR_B; /* so a fresh record lands in A */
  s_tick_rem = 0;
  s_last_persist_s = 0;
  s_prev_clean = false;
  s_time_synced = false;

  if (libtock_alarm_command_get_frequency(&s_freq) != 0 || s_freq == 0) {
    return ENTS_UPTIME_ERR_NO_CLOCK;
  }
  if (libtock_alarm_command_read(&s_last_ticks) != 0) {
    return ENTS_UPTIME_ERR_NO_CLOCK;
  }

  record_t loaded;
  fram_addr slot;
  if (record_load(&loaded, &slot)) {
    s_rec = loaded;
    s_last_slot = slot;

    // Dirty on arrival means the previous run never got to mark itself clean,
    // so it died to a reset, a brownout or a watchdog.
    s_prev_clean = (s_rec.flags & FLAG_DIRTY) == 0;
    if (!s_prev_clean) {
      s_rec.unclean_boots++;
    }
  }

  s_rec.boot_count++;
  // A new session. Cumulative carries over, session does not.
  s_rec.session_seconds = 0;
  s_rec.boot_epoch = 0;
  // FLAG_HEARTBEAT_VALID is deliberately left alone. It describes the stored
  // heartbeat_epoch, which is exactly what the next timesync needs to measure
  // the outage against. s_time_synced, set false above, is what tracks whether
  // the clock is usable right now.
  // Mark dirty for the duration of the run.
  s_rec.flags |= FLAG_DIRTY;

  s_init = true;
  return record_store();
}

ents_uptime_status ents_uptime_accumulate(void) {
  if (!s_init) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }
  accumulate();
  return ENTS_UPTIME_OK;
}

ents_uptime_status ents_uptime_tick(void) {
  if (!s_init) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }

  accumulate();

  // Only touch FRAM on the persist interval. The counters live in RAM between
  // writes, so a reset loses at most one interval of session time.
  if (s_rec.session_seconds - s_last_persist_s >=
      (uint32_t)ENTS_UPTIME_PERSIST_INTERVAL_S) {
    // Only refresh the heartbeat from a clock this boot has actually synced.
    // Writing the power on epoch here would destroy the previous boot's value.
    if (s_time_synced) {
      s_rec.heartbeat_epoch = epoch();
    }
    return record_store();
  }
  return ENTS_UPTIME_OK;
}

ents_uptime_status ents_uptime_time_synced(void) {
  if (!s_init) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }

  accumulate();

  uint32_t now = epoch();

  // Measure the outage. Only possible when the previous boot also reached a
  // timesync, because otherwise heartbeat_epoch is meaningless. Guard against a
  // clock that moved backwards, which would otherwise underflow.
  if ((s_rec.flags & FLAG_HEARTBEAT_VALID) && s_rec.heartbeat_epoch != 0 &&
      now >= s_rec.heartbeat_epoch) {
    s_rec.downtime_seconds += now - s_rec.heartbeat_epoch;
  }

  s_time_synced = true;
  s_rec.flags |= FLAG_HEARTBEAT_VALID;
  s_rec.boot_epoch = now;
  s_rec.heartbeat_epoch = now;

  return record_store();
}

ents_uptime_status ents_uptime_mark_clean(void) {
  if (!s_init) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }

  accumulate();

  if (s_time_synced) {
    s_rec.heartbeat_epoch = epoch();
  }
  s_rec.flags &= (uint16_t)~FLAG_DIRTY;

  return record_store();
}

ents_uptime_status ents_uptime_flush(void) {
  if (!s_init) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }

  accumulate();
  if (s_time_synced) {
    s_rec.heartbeat_epoch = epoch();
  }
  return record_store();
}

ents_uptime_status ents_uptime_get(ents_uptime_stats* out) {
  if (out == NULL) {
    return ENTS_UPTIME_ERR_NOT_INIT;
  }
  if (!s_init) {
    memset(out, 0, sizeof(*out));
    return ENTS_UPTIME_ERR_NOT_INIT;
  }

  // Fold in whatever has elapsed since the last call, so callers always see a
  // current number without having to time their reads.
  accumulate();

  out->boot_count = s_rec.boot_count;
  out->unclean_boots = s_rec.unclean_boots;
  out->session_seconds = s_rec.session_seconds;
  out->cumulative_seconds = s_rec.cumulative_seconds;
  out->downtime_seconds = s_rec.downtime_seconds;
  out->time_valid = s_time_synced;
  out->previous_clean = s_prev_clean;
  return ENTS_UPTIME_OK;
}

ents_uptime_status ents_uptime_erase(void) {
  uint8_t zeros[sizeof(record_t)] = {0};

  if (fram_write(ENTS_UPTIME_ADDR_A, zeros, sizeof(zeros)) != FRAM_OK ||
      fram_write(ENTS_UPTIME_ADDR_B, zeros, sizeof(zeros)) != FRAM_OK) {
    return ENTS_UPTIME_ERR_FRAM;
  }

  memset(&s_rec, 0, sizeof(s_rec));
  s_init = false;
  return ENTS_UPTIME_OK;
}
