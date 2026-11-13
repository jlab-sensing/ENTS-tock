/* vim: set sw=2 expandtab tw=80: */

#include <libents/controller/controller.h>
#include <libents/controller/modules/microsd.h>
#include <libents/proto/sensor.h>
#include <libents/storage/fifo.h>
#include <libents/user_config.h>
#include <libents/util/time.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>
#include <libtock/tock.h>
#include <stdio.h>
#include <ulog.h>

#include "lorawan.h"
#include "user_config.h"
#include "wifi.h"

#ifdef HARDCODED_USER_CONFIG
#include "hardcoded_user_config.h"
#endif  // HARDCODED_USER_CONFIG

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

/** Upload over WiFi when true, otherwise LoRaWAN. Set from the user config. */
static bool use_wifi = false;

/** Maximum LoRaWAN payload size. */
static const int lorawan_payload_size = 60;

/** Maximum WiFi payload size, limited by WiFiCommand.resp (222 bytes). */
static const int wifi_payload_size = 200;

/** Time between repeated timesync requests. */
static int timesync_retry_delay_ms = 10000;

/** Time between WiFi connection attempts. */
static const int wifi_retry_delay_ms = 10000;

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

  // start service after connected
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL);

  // Print warning when using TEST_USER_CONFIG
#ifdef TEST_USER_CONFIG
  ulog_warn("TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG

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

#ifdef HARDCODED_USER_CONFIG
  // Overwrite the config in FRAM so the webserver and sensors app see it too
  ulog_warn("HARDCODED_USER_CONFIG is enabled, ignoring webserver config");
  if (UserConfigSave(&hardcoded_user_config) != USERCONFIG_OK ||
      UserConfigLoad() != USERCONFIG_OK) {
    ulog_error("Could not save hardcoded user config");
  }
  ulog_info("Hardcoded user configuration:");
  ulog_info("-----------------------------");
  UserConfigPrint();
#else
  // Get update configuration from server
  UserConfigUpdateFromServer();
#endif  // HARDCODED_USER_CONFIG

  // Load bytes into userconfig buffer for the sensors app. Done after the
  // config is updated so the sensors app gets the current config.
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

  const UserConfiguration* cfg = UserConfigGet();
  use_wifi = (cfg->Upload_method == Uploadmethod_WiFi);

  // Reset esp32 and start webserver. With WiFi the main loop stops the
  // webserver, as the alarm based autostop can interrupt a WiFi command.
  ControllerDeviceReset();
  UserConfigStart(use_wifi ? 0 : userconfig_timeout_ms);

  // return codes
  int ret = 0;

  if (use_wifi) {
    ulog_info("Uploading with WiFi");

    // Retry forever, the webserver stays up so the config can be fixed
    while (wifi_init() < 0) {
      ulog_info("Retrying in %d ms", wifi_retry_delay_ms);
      libtocksync_alarm_delay_ms(wifi_retry_delay_ms);
    }

    while (wifi_timesync() < 0) {
      ulog_info("Retrying in %d ms", timesync_retry_delay_ms);
      libtocksync_alarm_delay_ms(timesync_retry_delay_ms);
    }
  } else {
    ulog_info("Uploading with LoRaWAN");

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
  }

  // Time to stop the webserver when uploading with WiFi
  uint32_t userconfig_stop_time = epoch() + userconfig_timeout_ms / 1000;

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
    // Stop webserver
    //

    if (use_wifi && UserConfigCurrentStatus() == USERCONFIG_STATE_ON &&
        epoch() >= userconfig_stop_time) {
      if (!UserConfigTryStop()) {
        // try again next loop
        userconfig_stop_time = epoch() + upload_interval / 1000;
      }
    }

    //
    // Upload data
    //

    uint16_t meas_in_buffer = fifo_buffer_len();

    if (meas_in_buffer > 1) {
      // batch into minium of 4 measurements
      while (meas_in_buffer > 1) {
        ulog_debug("Buffer has %d measurements", meas_in_buffer);

        // format payload
        uint8_t buffer[wifi_payload_size] = {};
        int len = get_payload(
            buffer, use_wifi ? wifi_payload_size : lorawan_payload_size);
        if (len != 0) {
          ulog_debug("Uploading %d bytes", len);

          stats.total++;
          if (use_wifi) {
            ret = wifi_upload(buffer, len);
          } else {
            ret = lorawan_upload(buffer, len);
          }

          if (ret < 0) {
            stats.failed++;
            ulog_error("Could not upload with %s (error: %d)",
                       use_wifi ? "WiFi" : "LoRaWAN", ret);

            // Reconnect and leave the rest of the buffer for the next upload
            if (use_wifi) {
              wifi_init();
              break;
            }
          } else {
            ulog_debug("Uploaded %d bytes with %s.", len,
                       use_wifi ? "WiFi" : "LoRaWAN");

            stats.bytes += len;
          }
        }

        meas_in_buffer = fifo_buffer_len();
      }
    } else if (use_wifi) {
      ulog_debug("Nothing to upload");
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

static int get_payload(uint8_t* buffer, int size) {
  ulog_trace("get_payload");

  // return codes
  int ret = 0;

  Metadata meta = {};
  SensorMeasurement meas[8] = {};
  const uint16_t max_meas = sizeof(meas) / sizeof(meas[0]);

  uint16_t length = fifo_buffer_len();
  if (length > max_meas) {
    length = max_meas;
  }

  // Number of measurements in the payload
  uint16_t count = 0;
  // Number of measurements read from the buffer, including malformed ones
  uint16_t consumed = 0;
  // Size of the payload with count measurements
  size_t payload_len = 0;
  // Set when the next measurement does not fit in the payload
  bool full = false;

  for (consumed = 0; consumed < length; consumed++) {
    uint8_t meas_len = 0;
    ret = fifo_peek(consumed, buffer, &meas_len);
    if (ret < 0) {
      ulog_error("Could not read from buffer (error: %d)", ret);
      break;
    }
    ulog_debug("Read %d bytes from buffer", meas_len);

    // decode measuremnet, malformed measurements are dropped with the rest
    ret = DecodeSensorMeasurement(buffer, meas_len, &meas[count]);
    if (ret < 0) {
      ulog_error("Could not decode measurement (error %d), malformed data?",
                 ret);
      continue;
    }

    size_t new_len = 0;
    ret = RepeatedSensorMeasurementsSize(meta, meas, count + 1, &new_len);
    if (ret < 0) {
      ulog_error("Could not find size of payload (error %d)", ret);
      break;
    }
    ulog_debug("Size of repeated sensor measurements: %d", new_len);

    // stop before the measurement that exceeds the size of the buffer
    if (new_len > (size_t)size) {
      ulog_debug("Over sensors size limit of %d.", size);
      full = true;
      break;
    }

    PrintSensorMeasurement(&meas[count]);
    payload_len = new_len;
    count++;
  }

  // A single measurement larger than the payload can never be sent, drop it
  // so it does not block the buffer
  if (count == 0 && full) {
    ulog_error("Measurement too large for a %d byte payload, dropping", size);
    consumed = 1;
  }

  int len = 0;
  if (count > 0) {
    ret = EncodeRepeatedSensorMeasurements(meta, meas, count, buffer,
                                           (size_t)size, &payload_len);
    if (ret < 0) {
      ulog_error("Could not encode %d repeated measurements (error %d)", count,
                 ret);
      return 0;
    }
    len = (int)payload_len;
  }

  // Clear uploaded and malformed measurements
  for (uint16_t j = 0; j < consumed; j++) {
    ret = fifo_drop();
    if (ret < 0) {
      ulog_error("Could not remove measurement index %d from buffer", j);
    }
  }
  return len;
}
