#include <libtock-sync/services/alarm.h>

#include <ulog.h>
#include <libents/sensors/ads1219.h>

int main(void) {
  ulog_info("ADS1219 Example");

  // Initialize the ADS1219 sensor
  if (ads1219_reset() != ADS1219_SUCCESS) {
    ulog_error("Failed to initialize ADS1219");
    return 1;
  }

  ulog_info("ADS1219 initialized successfully");

  // Read and print voltage and current measurements every 2 seconds
  while (1) {
    double voltage = 0.0;
    double current = 0.0;

    // Read voltage measurement
    if (ads1219_voltage(&voltage) == ADS1219_SUCCESS) {
      ulog_info("Voltage: %d mV", (int)(voltage * 1000));
    } else {
      ulog_error("Failed to read voltage");
    }

    // Read current measurement
    if (ads1219_current(&current) == ADS1219_SUCCESS) {
      ulog_info("Current: %d mV", (int)(current * 1000));
    } else {
      ulog_error("Failed to read current");
    }

    libtocksync_alarm_delay_ms(2000);
  }

  return 0;
}

