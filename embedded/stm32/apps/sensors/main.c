#include <libents/proto/sensor.h>
#include <libents/sensors/ads1219.h>
#include <libents/sensors/bme280/bme280_sensor.h>
#include <libents/user_config.h>
#include <libents/util/time.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <ulog.h>

static int core_service = 0;

static libtock_alarm_t sensor_alarm = {};

// buffer to share with core service
static char core_buf[256] __attribute__((aligned(256)));

// flag when ipc is don
static bool done = false;

static int meas_idx = 0;

/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(__attribute__((unused)) int pid, int len, int buf,
                         void* ud) {
  done = true;
}

/**
 * @brief Function prototype for measure functions
 *
 * @param data Location to store data
 *
 * @return Number of bytes in data
 */
typedef uint8_t (*SensorsPrototypeMeasure)(uint8_t* data, uint32_t ts,
                                           uint32_t idx);

int measure_sensor(SensorsPrototypeMeasure cb);

int load_userconfig(void);

int measure_sensor(SensorsPrototypeMeasure cb) {
  ulog_trace("measure_sensor");

  // payload format
  uint8_t* cmd = core_buf;
  uint8_t* length = core_buf + 1;
  uint8_t* data = core_buf + 2;

  // send measurement command
  *cmd = 2;

  // store measurement in buffer
  uint32_t ts = epoch();
  *length = cb(data, ts, meas_idx++);

  // check for errors in cb
  if (*length < 255) {
    // Send to core for upload and wait for completion
    ipc_notify_service(core_service);
    yield_for(&done);
    done = false;

    // Write to SD card
#ifdef SAVE_TO_MICROSD
    ControllerMicroSDSave(data, *length);
#endif
  }

  return *length;
}

int load_userconfig(void) {
  ulog_trace("load_userconfig");

  // payload format
  uint8_t* cmd = core_buf;
  uint8_t* length = core_buf + 1;
  uint8_t* data = core_buf + 2;

  *cmd = 1;

  ulog_info("[d] getting userconfig");

  // Send to core for upload
  ipc_notify_service(core_service);
  yield_for(&done);

  ulog_info("[d] userconfig got from core");

  UserConfigStatus uc_status = UserConfigLoadBytes(data, *length);

  done = false;

  return uc_status;
}

void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size) {
  snprintf(prefix, prefix_size, "Sensors\t");
}

int main() {
  // This is required to delay the sensors app to start after core
  libtocksync_alarm_delay_ms(500);

  // set logging level and prefix
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("=== App Initialized ===");

  // return code
  int ret = 0;

  // setup ipc for uplaods
  ret = ipc_discover("org.ents.core", &core_service);
  if (ret < 0) {
    ulog_fatal("No core service %d", ret);
    return -1;
  } else {
    ulog_info("Found core service");
  }

  // register ipc callback with buffer
  ipc_register_client_callback(core_service, ipc_callback, NULL);
  ipc_share(core_service, core_buf, 256);

  //
  // Get user config code
  //

  ulog_info("[d] before load user config");

  ret = load_userconfig();
  if (ret == USERCONFIG_OK) {
    // print current user config
    ulog_info("Current user config:");
    UserConfigPrint();
  } else {
    ulog_error("Could not load user config.");
  }

  const UserConfiguration* cfg = UserConfigGet();

  // currently not functional
  // NOTE for initializing state
  // FIFO_Init();

  // setup repeating measurement call
  uint32_t period_ms = (uint32_t)(cfg->Upload_interval * 1000);

  //
  // Infinite measurement loop
  //

  while (1) {
    static bool init = false;

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
#if !defined(USE_FLOW_METER_SENSOR) && !defined(USE_WATER_PRESSURE_SENSOR) && \
    !defined(USE_CAP_SOIL_SENSOR) && !defined(USE_PHYTOS31_SENSOR)
#define DEFAULT
#endif

#ifdef DEFAULT
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
        ulog_info("Measured Current");
      }
      // if (sensor == EnabledSensor_Teros12) {
      //   ulog_info("Teros12");
      //   measure_sensor(Teros12Measure);
      // }
      // if (sensor == EnabledSensor_Teros21) {
      //   measure_sensor(Teros21Measure);
      //   ulog_info("Teros21");
      // }
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

    // set initialized flag
    init = true;

    libtocksync_alarm_delay_ms(period_ms);
  }

  return 0;
}
