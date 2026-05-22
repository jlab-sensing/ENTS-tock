#include <ulog.h>
#include <libents/sensors/bme280/bme280_sensor.h>
#include <libtock-sync/services/alarm.h>

int main(void) {
  ulog_info("Example bme280 measurements");

  BME280Status status = BME280Init();
  if (status != BME280_STATUS_OK) {
    ulog_error("Error initializing bme280: %d", status);
    return -1;
  }

  while (1) {
    BME280Data data = {};

    status = BME280MeasureAll(&data);
    if (status != BME280_STATUS_OK) {
      ulog_error("Error measuring from bme280: %d", status);
      continue;
    }

    ulog_info("Pressure: %d, Temperature: %d, Humidity: %d", data.pressure,
            data.temperature, data.humidity);

    libtocksync_alarm_delay_ms(2000);
  }

  return 0;
}
