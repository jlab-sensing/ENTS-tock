#include <ulog.h>
#include <libents/user_config.h>

int main() {
  ulog_info("Manually saving user configuration.");

  const static UserConfiguration testConfig = {
    .logger_id = 200,
    .cell_id = 200,
    .Upload_method = Uploadmethod_WiFi,
    .Upload_interval = 10,
    .enabled_sensors_count = 2,
    .enabled_sensors = {EnabledSensor_Voltage, EnabledSensor_Current},
    // calibration values are taken from 2.2.3-033
    .Voltage_Slope = -0.00039326,
    .Voltage_Offset = 4.92916378e-05,
    .Current_Slope = -1.18693164e-10,
    .Current_Offset = 4.14518594e-05,
    .WiFi_SSID = "HARE_Lab",
    .WiFi_Password = "",
    .API_Endpoint_URL = "http://dirtviz.jlab.ucsc.edu/api/sensor/",
    // port is not used
    .API_Endpoint_Port = 80};



  // Load current configuration
  int status = 0;
  status = UserConfigLoad();
  if (status < 0) {
    ulog_error("Could not load user configuration!");
  } else {
    ulog_info("Current user configuration:");
    ulog_info("--- START ---");
    UserConfigPrint();
    ulog_info("--- END ---");
  }


 
  // Save test configuration
  status = UserConfigSave(&testConfig);
  if (status != USERCONFIG_OK) {
    ulog_error("Could not save user configuration!");
    return -1;
  }

  // Load after saving to verify
  status = UserConfigLoad();
  if (status != USERCONFIG_OK) {
    ulog_error("Could not load user configuration after saving!");
  }

  // Print current configuration
  ulog_info("New User Configuration:");
  ulog_info("--- START ---");
  UserConfigPrint();
  ulog_info("--- END ---");

  ulog_info("Done.");

  return 0;
}
