/* vim: set sw=2 expandtab tw=80: */

#include <libents/ALIA/filter.h>
#include <libents/controller/controller.h>
#include <libents/controller/modules/microsd.h>
#include <libents/proto/sensor.h>
#include <libents/storage/fifo.h>
#include <libents/user_config.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>
#include <libtock/tock.h>
#include <stdio.h>
#include <ulog.h>

#include "lorawan.h"
#include "user_config.h"

/** Must match the MicroSDCommand.log field, which is 240 bytes (239 chars plus
 * the NUL). Anything smaller truncates the line here, before the proto sees it,
 * and ulog_event_to_cstr() writes "LEVEL FILE:LINE: MESSAGE" into this buffer,
 * so a long __FILE__ eats into the space left for the message. */
#define LOG_LINE_SIZE 240

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
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(int pid, int len, int buf, void* ud);

#ifndef ALIA_ENABLED
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
#endif  // ALIA_ENABLED

/**
 * @brief Callback to add prefix to ulog messages.
 *
 * @param ev Pointer to ulog event.
 * @param prefix Pointer to prefix buffer.
 * @param prefix_size Size of prefix buffer.
 */
void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size);

#ifdef ALIA_ENABLED
/**
 * @brief Extracts the numeric payload of a SensorMeasurement as a double.
 *
 * The value is a protobuf oneof, so the active arm has to be selected on
 * which_value rather than read directly.
 *
 * @param meas Pointer to the decoded measurement.
 * @param out Pointer to receive the value.
 * @return true if a value arm was set, false if the oneof is empty.
 */
static bool measurement_value(const SensorMeasurement* meas, double* out);

/**
 * @brief Runs one freshly received measurement through ALIA and uploads it if
 *        ALIA decides it is worth transmitting.
 * @param buf Pointer to the encoded SensorMeasurement.
 * @param len Length of buf.
 * @param registry Per-stream ALIA state, keyed by sensor type.
 * @param defaults ALIA parameters used to create a new sensor stream.
 */
static void alia_process(const uint8_t* buf, uint8_t len,
                         ALIARegistry* registry,
                         const ALIAUserConfig* defaults);

/**
 * @brief Smallest change worth reporting for a given sensor type.

 * @param type Sensor type of the measurement.
 * @return Resolution for a sensor
 */
static double resolution_for(SensorType type);
#endif  // ALIA_ENABLED

void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size) {
  (void)ev;

  snprintf(prefix, prefix_size, "Core\t");
}

/**
 * @brief microlog output handler that forwards a log line to the esp32.
 *
 * @param ev Event to format.
 * @param arg Unused user argument from ulog_output_add().
 */
static void microsd_log_output(ulog_event* ev, void* arg) {
  (void)arg;

  // Guard against recursion. If anything reached from ControllerMicroSDLog()
  // ever logs, that log would re-enter this handler and recurse forever.
  static bool busy = false;
  if (busy) {
    return;
  }
  busy = true;

  // static to keep 128 bytes off the stack, matching microlog's own examples.
  // Safe despite being shared state because the busy guard above prevents
  // re-entry.
  static char line[LOG_LINE_SIZE];
  if (ulog_event_to_cstr(ev, line, sizeof(line)) == ULOG_STATUS_OK) {
    ControllerMicroSDLog(line, "core");
  }

  busy = false;
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
#ifdef TEST_USER_CONFIG
  // Print warning when using TEST_USER_CONFIG
  ulog_warn("TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG

#ifdef ALIA_ENABLED
  // One slot per measurement stream. A multi-quantity sensor reports each
  // quantity as its own SensorType, and those carry different units, so they
  // must not share a statistics window.
  ALIARegistry alia_registry;
  alia_registry_init(&alia_registry);

  ALIAUserConfig alia_defaults = {};
  alia_defaults.event_delta_threshold = 2;
  alia_defaults.base_heartbeat_hours = 1;
  alia_defaults.doubling_hours = 6;
  alia_defaults.max_heartbeat_hours = 24;
  alia_defaults.sample_rate = 10;
  alia_defaults.std_dev_window_hours = 12;
#endif
  // Initialize controller interface
  ControllerInit();

  // Setup sd card output
  ulog_output_id sd_output =
      ulog_output_add(microsd_log_output, NULL, ULOG_LEVEL_INFO);
  if (sd_output == ULOG_OUTPUT_INVALID) {
    // Almost always means ULOG_BUILD_EXTRA_OUTPUTS was not set when microlog
    // was compiled, see embedded/external/microlog/Makefile.
    ulog_error("Could not register microSD log output");
    return 1;
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

  network_ready = true;

  while (1) {
    ulog_trace("main loop");

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

#ifdef ALIA_ENABLED
      // ALIA has to see the measurement here, on the iteration it actually
      // arrives. This branch continues below, so anything downstream would
      // only ever observe a stale meas_buffer on a later timeout iteration.
      //
      // ALIA also decides transmission per reading, so the fifo is bypassed:
      // an admitted reading uploads immediately and a suppressed one is
      // dropped, represented by the run length on the next transmission.
      alia_process(meas_buffer, meas_buffer_length, &alia_registry,
                   &alia_defaults);
#else
      // store in buffer
      ret = fifo_put(meas_buffer, meas_buffer_length);
      if (ret < 0) {
        ulog_error("Could not store measurement in buffer");
      }
#endif
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

#ifndef ALIA_ENABLED
    uint16_t meas_in_buffer = fifo_buffer_len();
    // batch into minium of 4 measurements
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
            ulog_debug("Uploaded %d bytes with LoRaWAN.", len);

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
        ulog_error("Error sending heartbeat (error: %d)", ret);
      } else {
        stats.heartbeats++;
        ulog_debug("Heartbeat sent");
      }
    }

#endif
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

#ifdef ALIA_ENABLED
static bool measurement_value(const SensorMeasurement* meas, double* out) {
  switch (meas->which_value) {
    case SensorMeasurement_unsigned_int_tag:
      *out = (double)meas->value.unsigned_int;
      return true;
    case SensorMeasurement_signed_int_tag:
      *out = (double)meas->value.signed_int;
      return true;
    case SensorMeasurement_decimal_tag:
      *out = meas->value.decimal;
      return true;
    default:
      return false;
  }
}

static double resolution_for(SensorType type) {
  switch (type) {
    case SensorType_BME280_TEMP:
      return 0.01;
    case SensorType_BME280_PRESSURE:
      return 0.18;
    case SensorType_BME280_HUMIDITY:
      return 0.008;
    default:
      return 0.1;
  }
}

static void alia_process(const uint8_t* buf, uint8_t len,
                         ALIARegistry* registry,
                         const ALIAUserConfig* defaults) {
  ulog_trace("alia_process");

  SensorMeasurement meas = {};
  int ret = DecodeSensorMeasurement(buf, len, &meas);
  if (ret < 0) {
    ulog_error("Could not decode measurement for ALIA (error: %d)", ret);
    return;
  }

  double value = 0.0;
  if (!measurement_value(&meas, &value)) {
    ulog_error("Measurement has no value set, skipping ALIA");
    return;
  }

  ALIAStream* stream =
      alia_stream_get(registry, meas.type, defaults, resolution_for(meas.type));

  uint32_t rle = 0;
  if (stream != NULL) {
    // Read the run before should_log resets it.
    rle = stream->run.run_count;

    if (!should_log(value, &stream->welford, &stream->heartbeat, &stream->run,
                    &stream->config)) {
      ulog_debug("ALIA suppressed type %d (run length %u)", (int)meas.type,
                 (unsigned)stream->run.run_count);
      return;
    }
  } else {
    // Out of slots
    ulog_warn("ALIA has no free stream slot for type %d; uploading unfiltered",
              (int)meas.type);
  }

  meas.rle_count = rle;

  uint8_t buffer[60] = {};
  size_t encoded_len = 0;
  Metadata meta = {};
  SensorMeasurement single[1] = {meas};

  ret = EncodeRepeatedSensorMeasurements(meta, single, 1, buffer,
                                         sizeof(buffer), &encoded_len);
  if (ret < 0) {
    ulog_error("Could not encode single measurement (error %d)", ret);
    return;
  }

  ulog_debug("Uploading %d bytes (single triggering measurement)",
             (int)encoded_len);
  stats.total++;
  ret = lorawan_upload(buffer, encoded_len);
  if (ret < 0) {
    stats.failed++;
    ulog_error("Could not upload with LoRaWAN (error: %d)", ret);
    return;
  }
  stats.bytes += (int)encoded_len;
}
#endif  // ALIA_ENABLED

#ifndef ALIA_ENABLED
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
#endif  // ALIA_ENABLED
