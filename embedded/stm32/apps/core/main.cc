/* vim: set sw=2 expandtab tw=80: */

#include <libents/ALIA/filter.h>
#include <libents/controller/controller.h>
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
  int heartbeat;
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
#ifdef TEST_USER_CONFIG
  // Print warning when using TEST_USER_CONFIG
  ulog_warn("TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG

#ifdef ALIA_ENABLED
  WelfordState welfordState;
  HeartbeatState heartbeatState;
  RunState runState;
  ALIAUserConfig config;

  welford_init(&welfordState);
  heartbeatState.last_event_ts = 0;
  heartbeatState.has_logged = false;
  runState.run_count = 0;
  config.event_delta_threshold = 2;
  config.sensor_resolution = 0.1;
  config.base_heartbeat_hours = 1;
  config.doubling_hours = 6;
  config.max_heartbeat_hours = 24;
#endif
  // Initialize controller interface
  ControllerInit();

  // UserConfigStatus uc_status = UserConfigLoad();
  //// start user config interface
  // if (uc_status == USERCONFIG_OK) {
  //   // print current user config
  //   ulog_info("Current user configuration:");
  //   ulog_info("---------------------------");
  //   UserConfigPrint();
  // } else {
  //   ulog_error("Could not load user config.");
  // }

  // Load user config and start webservice with timeotu
  UserConfigStart(120 * 1000);

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

    uint16_t meas_in_buffer = fifo_buffer_len();
    // batch into minium of 4 measurements
#ifdef ALIA_ENABLED
    if (cmd == 2) {
      SensorMeasurement meas = {};
      ret = DecodeSensorMeasurement(meas_buffer, meas_buffer_length, &meas);
      if (ret < 0) {
        ulog_error("Could not decode measurement for ALIA (error: %d)", ret);
      } else {
        double value = meas.value;  // match upcoming changes to sensor
                                    // measurement name with proto
        uint32_t rle = runState.run_count;
        bool transmit = should_log(value, &welfordState, &heartbeatState,
                                   &runState, &config);

        if (transmit) {
          heartbeatState.last_transmitted_value = value;
          meas.rle_count = rle;
          uint8_t buffer[60] = {};
          int len = 0;
          Metadata meta = {};
          SensorMeasurement single[1] = {meas};

          ret = EncodeRepeatedSensorMeasurements(meta, single, 1, buffer,
                                                 sizeof(buffer), (size_t*)&len);
          if (ret < 0) {
            ulog_error("Could not encode single measurement (error %d)", ret);
          } else {
            ulog_debug("Uploading %d bytes (single triggering measurement)",
                       len);
            stats.total++;
            ret = lorawan_upload(buffer, len);
            if (ret < 0) {
              stats.failed++;
              ulog_error("Could not upload with LoRaWAN (error: %d)", ret);
            } else {
              stats.bytes += len;
            }
          }
        }
      }
    }
#else
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
        stats.heartbeat++;
        ulog_debug("Heartbeat sent");
      }
    }

#endif
    //
    // print stats
    //
    if (!(stats.total % 6)) {
      ulog_info(
          "total uploads: %d\tfailed uploads: %d\tmeasurements: %d\tbytes: "
          "%d\t",
          stats.total, stats.failed, stats.meas, stats.bytes);
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
