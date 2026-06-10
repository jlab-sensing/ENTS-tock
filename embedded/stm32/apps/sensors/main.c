#include <stdio.h>
#include <stdbool.h>

#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>

#include <libents/proto/sensor.h>
#include <libents/sensors/ads1219.h>
#include <libents/sensors/bme280/bme280_sensor.h>
#include <libents/sensors/sensors.h>

#include <libents/user_config.h>

#include <ulog.h>



void ulog_prefix_handler(ulog_event *ev, char *prefix, size_t prefix_size) {
  snprintf(prefix, prefix_size, "Sensors\t");
}



int main() { 
  // setup logging output
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);

  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("App Initialized");


  // return code
  int ret = 0;


  // Print warning when using TEST_USER_CONFIG
#ifdef TEST_USER_CONFIG
  ulog_notice("TEST_USER_CONFIG is enabled!");
#endif  // TEST_USER_CONFIG
  
  // set upload interval
  //UserConfigStatus uc_status = UserConfigLoad();
  //ulog_info("User Config Load: %d", uc_status);
  const UserConfiguration *cfg = UserConfigGet();


  // currently not functional
  // NOTE for initializing state
  // FIFO_Init();

  ulog_info("Enabled Sensors");
  ulog_info("---------------");
  
  printf("\n\n");
  UserConfigPrintAny(cfg);
  printf("\n\n");



  while (1) { yield(); }



  // configure enabled sensors
  for (int i = 0; i < cfg->enabled_sensors_count; i++) {
    printf("\n\n\n\n");
    ulog_info("HERE %d", i);
    printf("\n\n\n\n");

    EnabledSensor sensor = cfg->enabled_sensors[i];

    if (sensor == EnabledSensor_Voltage || sensor == EnabledSensor_Current) {
      ret = ads1219_reset();
      if (ret < 0) {
        ulog_error("Could not reset ads1219.");
      }
    }

    // Voltage channel is used by multiple different sensors
    if (sensor == EnabledSensor_Voltage) {

#ifdef DEFAULT
      SensorAdd(ads1219_sensor_voltage);
      ulog_info("Voltage Enabled!");
#endif

#ifdef USE_FLOW_METER_SENSOR
      FlowInit();
      SensorsAdd(WatFlow_measure);
      ulog_info("Flow Meter Enabled!");
#endif

#ifdef USE_WATER_PRESSURE_SENSOR
      PressureInit();
      SensorsAdd(WatPress_measure);
      ulog_info("Water Pressure Sensor Enabled!");
#endif

#ifdef USE_CAP_SOIL_SENSOR
      CapSoilInit();
      SensorsAdd(SEN0308_measure);
      ulog_info("Cap Soil Sensor Enabled!");
#endif

#ifdef USE_PHYTOS31_SENSOR

#endif
    }

    if (sensor == EnabledSensor_Current) {
      SensorsAdd(ads1219_sensor_current);
      ulog_info("Current Enabled!");
    }
    //if (sensor == EnabledSensor_Teros12) {
    //  ulog_info("Teros12 Enabled!");
    //  SensorsAdd(Teros12Measure);
    //}
    //if (sensor == EnabledSensor_Teros21) {
    //  SensorsAdd(Teros21Measure);
    //  ulog_info("Teros21 Enabled!");
    //}
    if (sensor == EnabledSensor_BME280) {
      BME280Init();
      SensorsAdd(BME280Measure);
      ulog_info("BME280 Enabled!");
    }
    // if (sensor == EnabledSensor_Phytos31) {
    //   Phytos31Init();
    //   SensorsAdd(Phytos31_measure);
    //   ulog_info("Phytos31 Enabled!");
    // }
    // if (sensor == EnabledSensor_SEN0308) {
    //   CapSoilInit();
    //   SensorsAdd(SEN0308_measure);
    //   ulog_info("SEN0308 Cap Soil Sensor Enabled!");
    // }
    // if (sensor == EnabledSensor_SEN0257) {
    //   PressureInit();
    //   SensorsAdd(WatPress_measure);
    //   ulog_info("SEN0257 Water Pressure Sensor Enabled!");
    // }
    // if (sensor == EnabledSensor_YFS210C) {
    //   FlowInit();
    //   SensorsAdd(WatFlow_measure);
    //   ulog_info("YFS210C Flow Meter Enabled!");
    // }
    //if (sensor == EnabledSensor_PCAP02) {
    //  pcap02_init();
    //  SensorsAdd(pcap02_measure);
    //  ulog_info("PCAP02 Enabled!");
    //}
    // TODO add support for dummy sensor
  }

  

 


  // setup ipc for uplaods
  int core_service = 0;
  ret = ipc_discover("org.ents.core", &core_service);
  if (ret < 0) {
    ulog_fatal("No core service %d", ret);
    return -1;
  }




  // convert to ms
  uint32_t period_ms = (uint32_t) (cfg->Upload_interval * 1000);
  SensorsStart(core_service, period_ms);



  while (1) {
    yield();
  }

  return 0;
}
