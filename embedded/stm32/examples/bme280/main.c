#include <ulog.h>
#include <libents/sensors/bme280/bme280_sensor.h>
#include <libtock-sync/services/alarm.h>

int main(void) {
  ulog_info("Example bme280 measurements");
  
#ifdef BME280_DOUBLE_ENABLE
  ulog_info("BME280_DOUBLE_ENABLE");
#endif

#ifdef BME280_64BIT_ENABLE
  ulog_info("BME280_64BIT_ENABLE");
#endif

#ifdef BME280_32BIT_ENABLE
  ulog_info("BME280_32BIT_ENABLE");
#endif



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
    
    printf("p: %d, t: %d, h: %d\n", data.pressure, data.temperature, data.humidity);

    libtocksync_alarm_delay_ms(2000);
  }

  return 0;
}
