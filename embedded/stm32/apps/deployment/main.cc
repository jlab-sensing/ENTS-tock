/* vim: set sw=2 expandtab tw=80: */

#include <libents/controller/controller.h>
#include <libents/controller/modules/microsd.h>
#include <libents/proto/sensor.h>
#include <libents/sensors/ads1219.h>
#include <libents/sensors/bme280/bme280_sensor.h>
#include <libents/sensors/teros12.h>
#include <libents/storage/fifo.h>
#include <libents/user_config.h>
#include <libents/util/time.h>
#include <libtock-sync/services/alarm.h>
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

// Voltage channel is used by multiple different sensors, default to reading
// the raw voltage when no other sensor is selected
#if !defined(USE_FLOW_METER_SENSOR) && !defined(USE_WATER_PRESSURE_SENSOR) && \
    !defined(USE_CAP_SOIL_SENSOR) && !defined(USE_PHYTOS31_SENSOR)
#define USE_DEFAULT_VOLTAGE_SENSOR
#endif

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

/** Index of the next measurement */
static uint32_t meas_idx = 0;

/** Time between repeated timesync requests. */
static int timesync_retry_delay_ms = 10000;

/** Time before user config webserver is turned off */
static const int userconfig_timeout_ms = 300 * 1000;

/**
 * @brief Function prototype for measure functions
 *
 * @param data Location to store data
 * @param meta Metadata to attach to the measurement
 * @param idx Index of the measurement
 *
 * @return Number of bytes in data
 */
typedef uint8_t (*SensorsPrototypeMeasure)(uint8_t* data, Metadata meta,
                                           uint32_t idx);

/**
 * @brief Takes a single measurement and stores it in the fifo for upload.
 *
 * @param cb Sensor measure function.
 * @return Number of bytes in the measurement.
 */
static int measure_sensor(SensorsPrototypeMeasure cb);

/**
 * @brief Measures all sensors enabled in the user config.
 *
 * @param init True if sensors have already been initialized.
 */
static void measure_sensors(bool init);

/**
 * @brief Uploads all buffered measurements, or a heartbeat if there are none.
 */
static void upload(void);

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

  snprintf(prefix, prefix_size, "Deployment\t");
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
    ControllerMicroSDLog(line, "deployment");
  }

  busy = false;
}

int main(void) {
  // Setup logging level and prefix
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("=== Deployment App Initialized ===");

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

  const UserConfiguration* cfg = UserConfigGet();

  // currently not functional
  // NOTE for initializing state
  // FIFO_Init();

  // period between measurements and uploads
  uint32_t period_ms = cfg->Upload_interval * 1000;

  //
  // Infinite measurement loop
  //

  bool init = false;

  while (1) {
    ulog_trace("main loop");

    measure_sensors(init);
    init = true;

    upload();

    //
    // print stats
    //
    if (!(stats.total % 6)) {
      ulog_info(
          "total uploads: %d  failed uploads: %d  measurements: %d  bytes: "
          "%d  heartbeats: %d",
          stats.total, stats.failed, stats.meas, stats.bytes, stats.heartbeats);
    }

    libtocksync_alarm_delay_ms(period_ms);
  }
}

static int measure_sensor(SensorsPrototypeMeasure cb) {
  ulog_trace("measure_sensor");

  // buffer to store measurement
  uint8_t data[256] = {};

  // get metadata
  const UserConfiguration* cfg = UserConfigGet();

  Metadata meta = {};
  meta.ts = epoch();
  meta.logger_id = cfg->logger_id;
  meta.cell_id = cfg->cell_id;

  // store measurement in buffer
  uint8_t length = cb(data, meta, meas_idx++);

  // check for errors in cb
  if (length < 255) {
    ulog_info("Measured %d bytes", length);

    // store in buffer for upload
    int ret = fifo_put(data, length);
    if (ret < 0) {
      ulog_error("Could not store measurement in buffer");
    }
    stats.meas++;

    // Write to SD card
#ifdef SAVE_TO_MICROSD
    ControllerMicroSDSave(data, length);
#endif
  }

  return length;
}

static void measure_sensors(bool init) {
  ulog_trace("measure_sensors");

  // return code
  int ret = 0;

  const UserConfiguration* cfg = UserConfigGet();

  // configure enabled sensors
  for (int i = 0; i < cfg->enabled_sensors_count; i++) {
    EnabledSensor sensor = cfg->enabled_sensors[i];

    if (sensor == EnabledSensor_Voltage || sensor == EnabledSensor_Current) {
      if (!init) {
        ret = ads1219_reset();
        if (ret < 0) {
          ulog_error("Could not reset ads1219.");
        }
      }
    }

    // Voltage channel is used by multiple different sensors
    if (sensor == EnabledSensor_Voltage) {
#ifdef USE_DEFAULT_VOLTAGE_SENSOR
      measure_sensor(ads1219_sensor_voltage);
      ulog_info("Measured Voltage");
#endif

#ifdef USE_FLOW_METER_SENSOR
      if (!init) {
        FlowInit();
        ulog_info("Initialized flow meter");
      }
      measure_sensor(WatFlow_measure);
      ulog_info("Measured Flow Meter");
#endif

#ifdef USE_WATER_PRESSURE_SENSOR
      if (!init) {
        PressureInit();
        ulog_info("Initialized Water Pressure");
      }
      measure_sensor(WatPress_measure);
      ulog_info("Measured Water Pressure Sensor");
#endif

#ifdef USE_CAP_SOIL_SENSOR
      if (!init) {
        CapSoilInit();
        ulog_info("Initialized Cap Soil");
      }
      measure_sensor(SEN0308_measure);
      ulog_info("Measured Cap Soil Sensor");
#endif

#ifdef USE_PHYTOS31_SENSOR

#endif
    }

    if (sensor == EnabledSensor_Current) {
      measure_sensor(ads1219_sensor_current);
      ulog_info("Measured current");
    }
    if (sensor == EnabledSensor_Teros12) {
      // TODO (jmadden173): Partially ported code for multiple sensors
      // const UserConfiguration *cfg = UserConfigGet();
      // uint32_t sensor_index = cfg->enabled_sensors_multiple[idx].index;

      //// SDI-12 spec 1.4: 0-9 (48-57), A-Z (65-90), a-z (97-122)
      // char sdi12_address = '0';

      // switch (sensor_index) {
      //  case 0 ... 9:  // default address is '0'. Also fix common user error
      //  of not
      //                 // putting the ascii decimal for '0'-'9'.
      //    sdi12_address = sensor_index + '0';
      //    break;
      //  case '0' ... '9':
      //  case 'A' ... 'Z':
      //  case 'a' ... 'z':
      //    sdi12_address = sensor_index;
      //    break;
      //  default:
      //    APP_LOG(TS_ON, VLEVEL_H,
      //            "Invalid SDI-12 address provided in the userconfig index
      //            field: " "0x%X ('%c')\r\n", sensor_index, sensor_index);
      //    return -1;
      //    break;
      //}

      //// metadata
      // Metadata meta = Metadata_init_zero;
      // meta.ts = ts.Seconds;
      // meta.logger_id = cfg->logger_id;
      // if (cfg->enabled_sensors_multiple[idx].cell_id != 0) {
      //   meta.cell_id = cfg->enabled_sensors_multiple[idx].cell_id;
      // } else {
      //   meta.cell_id = cfg->cell_id;
      // }

      Teros12Measure('0');
      measure_sensor(Teros12MeasureVWC);
      measure_sensor(Teros12MeasureVWCRaw);
      measure_sensor(Teros12MeasureTemp);
      measure_sensor(Teros12MeasureEC);

      ulog_info("Measured Teros12");
    }
    // if (sensor == EnabledSensor_Teros21) {

    //  ulog_info("Teros21");
    //}
    if (sensor == EnabledSensor_BME280) {
      if (!init) {
        BME280Init();
        ulog_info("Initialized BME280");
      }
      measure_sensor(BME280MeasureTemperature);
      measure_sensor(BME280MeasurePressure);
      measure_sensor(BME280MeasureHumidity);
      ulog_info("Measured BME280");
    }
    // if (sensor == EnabledSensor_Phytos31) {
    //   Phytos31Init();
    //   measure_sensor(Phytos31_measure);
    //   ulog_info("Phytos31");
    // }
    // if (sensor == EnabledSensor_SEN0308) {
    //   CapSoilInit();
    //   measure_sensor(SEN0308_measure);
    //   ulog_info("SEN0308 Cap Soil Sensor");
    // }
    // if (sensor == EnabledSensor_SEN0257) {
    //   PressureInit();
    //   measure_sensor(WatPress_measure);
    //   ulog_info("SEN0257 Water Pressure Sensor");
    // }
    // if (sensor == EnabledSensor_YFS210C) {
    //   FlowInit();
    //   measure_sensor(WatFlow_measure);
    //   ulog_info("YFS210C Flow Meter");
    // }
    // if (sensor == EnabledSensor_PCAP02) {
    //  pcap02_init();
    //  measure_sensor(pcap02_measure);
    //  ulog_info("PCAP02");
    //}
    // TODO add support for dummy sensor
  }
}

static void upload(void) {
  ulog_trace("upload");

  // return codes
  int ret = 0;

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
