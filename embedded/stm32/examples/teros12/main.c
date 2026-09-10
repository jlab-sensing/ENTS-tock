/**
 * @brief Reads a Teros12 sensor and prints measurements.
 *
 * VWC and temperature are both stored as double to represent the actual
 * measurements. Prints are only shown as integers because compiled version of
 * newlib lacks floating point print support.
 *
 * @author John Madden (jmadden173@pm.me)
 * @date 2026-09-03
 */

#include <libents/sensors/teros12.h>
#include <libtock-sync/services/alarm.h>
#include <ulog.h>

int main(void) {
  ulog_info("Teros12 Example");

  const int meas_interval_ms = 2000;

  // Read and print voltage and current measurements every 2 seconds
  while (1) {
    int ret = 0;

    ret = Teros12Measure('0');
    if (ret < 0) {
      ulog_error("Failed to read Teros12");
    }

    Teros12Data data = Teros12GetMeasurement();

    ulog_info("addr: %c, vwc: %d, temp: %d, ec: %d", data.addr, (int) data.vwc, (int) data.temp, data.ec);

    libtocksync_alarm_delay_ms(meas_interval_ms);
  }

  return 0;
}
