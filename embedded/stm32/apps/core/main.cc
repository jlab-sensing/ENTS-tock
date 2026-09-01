/* vim: set sw=2 expandtab tw=80: */

#include <libents/controller/controller.h>
#include <libents/proto/sensor.h>
#include <libents/storage/fifo.h>
#include <libents/user_config.h>
#include <libents/util/time.h>
#include <libents/util/uptime.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>
#include <libtock/tock.h>
#include <stdio.h>
#include <ulog.h>

#include "lorawan.h"
#include "user_config.h"

/** Stats for uploads */
typedef struct {
  /** Total number of bytes */
  int bytes;
  /** Total number uploads */
  int total;
  /** Failed uploads */
  int failed;
  /** Total number of measurements */
  int meas;
  /** Number of heartbeats */
  int heartbeats;
} upload_stats;

upload_stats stats = {};

// buffer to store measurements
static uint8_t meas_buffer[256] = {};
static uint8_t meas_buffer_length = 0;

// buffer for user config
static uint8_t uc_buffer[256] = {};
static uint8_t uc_buffer_length = 0;

static uint8_t cmd = 0;

// last pid command
static int last_pid = 0;

static bool has_data = false;

static bool network_ready = false;

/** Time between repeated timesync requests. */
static int timesync_retry_delay_ms = 10000;

/** Time between upload intervals. */
static int upload_interval = 60000;

/** Time before user config webserver is turned off */
static const int userconfig_timeout_ms = 300 * 1000;

/**
 * Send the device health counters once every this many uploads.
 *
 * The LoRaWAN payload budget is 60 bytes and a SensorMeasurement runs roughly
 * 15 to 25 encoded, so sending all five counters every batch would crowd out
 * real measurements.
 */
#define UPTIME_REPORT_EVERY 12

/** stats.total at the last health report, drives the report interval. */
static int last_uptime_report = 0;

/**
 * @brief Queue the device health counters for upload.
 *
 * Encodes each counter as an ordinary SensorMeasurement and pushes it into the
 * same FIFO the sensor apps use, so it rides the existing batching and uplink
 * with no special handling anywhere downstream.
 *
 * @return Number of counters queued.
 */
static int report_uptime(void);

/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(int pid, int len, int buf, void* ud);

/**
 * @brief Gets a formatted sensor measurement payload.
 *
 *
 * Peeks into the fram circular buffer and decodes Measurements until the size
 * of a RepeatedSensorMeasurements exceeds the buffer size. Then it encodes
 * RepeatedSensorMeasurements with (n-1) measurements and returns that back.
 *
 * @param buffer Pointer to buffer.
 * @param sizes Size of buffer.
 * @return len Number of bytes in buffer.
 */
static int get_payload(uint8_t* buffer, int size);

/**
 * @brief Callback to add prefix to ulog messages.
 *
 * @param ev Pointer to ulog event.
 * @param prefix Pointer to prefix buffer.
 * @param prefix_size Size of prefix buffer.
 */
void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size);

void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size) {
  (void)ev;

  snprintf(prefix, prefix_size, "Core\t");
}

int main(void) {
  // Setup logging level and prefix
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("=== Core App Initialized ===");

  // Load bytes into userconfig buffer
  //
  // Yes I am casting uint8_t to a uint16_t and it could overwrite the data
  // buffer. I know at the time of writing this that the user config stays
  // under 256 bytes as defined by protobuf.
  UserConfigStatus uc_status =
      UserConfigBytes(uc_buffer, (uint16_t*)&uc_buffer_length);
  if (uc_status != USERCONFIG_OK) {
    ulog_error("Could not load user config.");
  }

  // Option to print bytes to the buffer
  // printf("uc_buffer[%u]:", uc_buffer_length);
  // for (uint8_t i = 0; i < uc_buffer_length; i++) {
  //  printf(" %02x", uc_buffer[i]);
  //}
  // printf("\n\n");

  // start service after connected
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL);

  // Print warning when using TEST_USER_CONFIG
#ifdef TEST_USER_CONFIG
  ulog_warn("TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG

  // Initialize controller interface
  ControllerInit();

  // Count this boot and start the session timer. Deliberately before the
  // network comes up: an unclean boot has to be recorded even if the node
  // never manages to join, since a node stuck in a join loop is exactly the
  // failure these counters exist to catch.
  ents_uptime_status up_status = ents_uptime_init();
  if (up_status != ENTS_UPTIME_OK) {
    ulog_error("Uptime tracking unavailable (error: %d)", (int)up_status);
  } else {
    ents_uptime_stats up = {};
    ents_uptime_get(&up);
    ulog_info("Boot %lu, %lu unclean, previous shutdown %s",
              (unsigned long)up.boot_count, (unsigned long)up.unclean_boots,
              up.previous_clean ? "clean" : "UNCLEAN");
  }

  // Get update configuration from server
  UserConfigUpdateFromServer();

  // Reset esp32 and start webserver
  ControllerDeviceReset();
  UserConfigStart(userconfig_timeout_ms);

  // return codes
  int ret = 0;

  // Initialize LoRaWAN

  ret = lorawan_init();
  if (ret < 0) {
    return ret;
  }

  ret = lorawan_join();
  if (ret < 0) {
    return ret;
  }

  while (1) {
    ret = lorawan_timesync();
    if (ret < 0) {
      ulog_info("Retrying in %d ms", timesync_retry_delay_ms);
      libtocksync_alarm_delay_ms(timesync_retry_delay_ms);
    } else {
      break;
    }
  }

  // The clock is only trustworthy now. On the measured hardware the RTC reads
  // 2000-01-01 until this point, so this is where an outage across the reset
  // becomes measurable, not in the boot sequence.
  if (ents_uptime_time_synced() == ENTS_UPTIME_OK) {
    ents_uptime_stats up = {};
    ents_uptime_get(&up);
    if (up.downtime_seconds > 0) {
      ulog_info("Measured downtime so far: %lu s",
                (unsigned long)up.downtime_seconds);
    }
  }

  network_ready = true;

  while (1) {
    ulog_trace("main loop");

    // Fold elapsed time into the counters and persist on its own schedule.
    // The loop is guaranteed to run at least every upload_interval, which is
    // far inside both the 74 hour tick counter wrap and the FRAM persist
    // interval, so no separate alarm is needed to keep the count honest.
    ents_uptime_tick();

    ret = libtocksync_alarm_yield_for_with_timeout(&has_data, upload_interval);
    if (ret == 0) {
      ulog_info("Loop triggered with condition");
    } else if (ret == -1) {
      ulog_info("Loop triggered with timeout");
    } else {
      ulog_warn("Unknown behavior with yield_for_with_timeout (error: %d)",
                ret);
    }

    //
    // Save data on matched command
    //

    if (has_data && cmd == 2) {
      // print out bytes
      //  Get number of bytes in buffer
      ulog_info("Received %d bytes:", meas_buffer_length);
      // for (int i=1; i < meas_buffer_length; i++) {
      //   printf("%x ", meas_buffer[i]);
      // }
      // printf("\n");

      // store in buffer
      ret = fifo_put(meas_buffer, meas_buffer_length);
      if (ret < 0) {
        ulog_error("Could not store measurement in buffer");
      }
      stats.meas++;

      // indicate data has been processed and trigger client
      has_data = false;
      ipc_notify_client(last_pid);

      // skip uploads if received ipc call
      continue;
    }

    //
    // Upload data
    //

    // Queue the health counters occasionally so they ride the next batch.
    if (stats.total - last_uptime_report >= UPTIME_REPORT_EVERY) {
      last_uptime_report = stats.total;
      report_uptime();
    }

    uint16_t meas_in_buffer = fifo_buffer_len();

    if (meas_in_buffer > 1) {
      // batch into minium of 4 measurements
      while (meas_in_buffer > 1) {
        ulog_debug("Buffer has %d measurements", meas_in_buffer);

        // format payload
        uint8_t buffer[60] = {};
        int len = get_payload(buffer, sizeof(buffer));
        if (len != 0) {
          ulog_debug("Uploading %d bytes", len);

          stats.total++;
          ret = lorawan_upload(buffer, len);
          if (ret < 0) {
            stats.failed++;
            ulog_error("Could not upload with LoRaWAN (error: %d)", ret);
          } else {
            ulog_debug("Uploaded %d bytes with LoRaWAN.");

            stats.bytes += len;
          }
        }

        meas_in_buffer = fifo_buffer_len();
      }
    } else {
      stats.total++;

      ulog_debug("Sending heartbeat");

      ret = lorawan_heartbeat();
      if (ret < 0) {
        stats.failed++;
        ulog_error("Error sending heartbeat (error: %d)");
      } else {
        stats.heartbeats++;
        ulog_debug("Heartbeat sent");
      }
    }

    //
    // print stats
    //
    if (!(stats.total % 6)) {
      ulog_info(
          "total uploads: %d  failed uploads: %d  measurements: %d  bytes: "
          "%d  heartbeats: %d",
          stats.total, stats.failed, stats.meas, stats.bytes, stats.heartbeats);
    }
  }
}

static void ipc_callback(int pid, int len, int buf, void* ud) {
  (void)len;
  (void)ud;

  ulog_trace("ipc_callabck");

  uint8_t* buffer = (uint8_t*)buf;

  // payload format
  cmd = buffer[0];
  uint8_t* length = buffer + 1;
  uint8_t* data = buffer + 2;

  // Reply with userconfig when requested
  if (cmd == 1) {
    ulog_trace("user config command");

    // copy from user config to data
    memcpy(data, uc_buffer, uc_buffer_length);
    *length = uc_buffer_length;

    // trigger client
    ipc_notify_client(pid);

    // Store measurements into buffer
  } else if (cmd == 2) {
    ulog_trace("measurement command");

    // copy data to buffer
    meas_buffer_length = *length;
    memcpy(meas_buffer, data, *length);

    // store last pid so it can be triggered
    // basically hold sensors in an interrupt until we store the measurement
    last_pid = pid;
    has_data = true;

    // ready command
  } else if (cmd == 3) {
    ulog_trace("ready command");

    buffer[1] = (uint8_t)network_ready;

    // trigger client
    ipc_notify_client(pid);

    // Catch all other commands
  } else {
    ulog_error("IPC command %d not implemented.", buffer[0]);
  }
}

static int report_uptime(void) {
  ents_uptime_stats up = {};
  if (ents_uptime_get(&up) != ENTS_UPTIME_OK) {
    return 0;
  }

  const UserConfiguration* uc = UserConfigGet();
  if (uc == NULL) {
    ulog_warn("No user config, cannot label uptime counters");
    return 0;
  }

  Metadata meta = {};
  meta.cell_id = uc->cell_id;
  meta.logger_id = uc->logger_id;
  meta.ts = epoch();

  // Boot counters change rarely, so they are worth sending every time this
  // runs. The two second counters are the ones that move constantly.
  const struct {
    SensorType type;
    uint32_t value;
  } counters[] = {
      {SensorType_DEVICE_BOOT_COUNT, up.boot_count},
      {SensorType_DEVICE_UNCLEAN_BOOTS, up.unclean_boots},
      {SensorType_DEVICE_UPTIME, up.session_seconds},
      {SensorType_DEVICE_CUMULATIVE_UPTIME, up.cumulative_seconds},
      {SensorType_DEVICE_DOWNTIME, up.downtime_seconds},
  };

  int queued = 0;
  for (size_t i = 0; i < sizeof(counters) / sizeof(counters[0]); i++) {
    uint8_t buffer[64] = {};
    size_t len = sizeof(buffer);

    if (EncodeUint32Measurement(meta, counters[i].value, counters[i].type,
                                buffer, &len) != SENSOR_OK) {
      ulog_error("Could not encode uptime counter %d", (int)counters[i].type);
      continue;
    }
    if (fifo_put(buffer, (uint8_t)len) < 0) {
      // Buffer full is not fatal here. Health counters are the first thing that
      // should be dropped when the FIFO is under pressure from real data.
      ulog_warn("Buffer full, dropping uptime counter %d",
                (int)counters[i].type);
      continue;
    }
    queued++;
  }

  ulog_debug("Queued %d uptime counters", queued);
  return queued;
}

static int get_payload(uint8_t* buffer, int size) {
  ulog_trace("get_payload");

  // return codes
  int ret = 0;

  int len = 0;

  Metadata meta = {};
  SensorMeasurement meas[8] = {};

  uint16_t length = fifo_buffer_len();

  uint16_t i = 0;
  for (i = 0; i < length; i++) {
    ret = fifo_peek(i, buffer, (uint8_t*)&len);
    if (ret < 0) {
      ulog_error("Could not read from buffer (error: %d)", ret);
      continue;
    }
    ulog_debug("Read %d bytes from buffer", len);

    // decode measuremnet
    ret = DecodeSensorMeasurement(buffer, len, &meas[i]);
    if (ret < 0) {
      ulog_error("Could not decode measurement (error %d), malformed data?",
                 ret);
      // TODO: Removed failing measurement. Can't use drop, need to remove
      // specific index.
      continue;
    }

    ret = RepeatedSensorMeasurementsSize(meta, meas, i + 1, (size_t*)&len);
    if (ret < 0) {
      ulog_error("Could not find size of payload (error %d)", ret);
      continue;
    }
    ulog_debug("Size of repeated sensor measurements: %d", len);

    // early stop when length exceeds size of buffer
    if (len > size) {
      ulog_debug("Over sensors size limit of %d. Removing last measurement.",
                 size);

      i--;
      break;
    }

    PrintSensorMeasurement(&meas[i]);
  }

  ret = EncodeRepeatedSensorMeasurements(meta, meas, i + 1, buffer, size,
                                         (size_t*)&len);
  if (ret < 0) {
    ulog_error("Could not encode %d repeated measurements (error %d)", i, ret);
    return 0;
  }

  // Clear uploaded measurements
  // ulog_debug("%d measurements to drop");
  for (uint16_t j = 0; j < i; j++) {
    ret = fifo_drop();
    if (ret < 0) {
      ulog_error("Could not remove measurement index %d from buffer", j);
    }
  }
  return len;
}
