#include <stdio.h>
#include <stdbool.h>

#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>

#include <proto/sensor.h>
#include <sensors/ads1219.h>

static bool done = false;

/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(__attribute__ ((unused)) int pid, int len, int buf, void* ud) {
  done = true;
}


char core_buf[256] __attribute__((aligned(256)));

int main() {
  libtocksync_alarm_delay_ms(500);
  printf("Sensor App Starting\n");

  // return codes
  int ret = 0;


  // setup ipc for uplaods
  int core_service = 0;

  ret = ipc_discover("org.ents.core", &core_service);
  if (ret < 0) {
    printf("No core service %d\n", ret);
    return -1;
  }

  ipc_register_client_callback(core_service, ipc_callback, NULL);
  ipc_share(core_service, core_buf, 256);

  // reset to known state
  ret = ads1219_reset();
  if (ret < 0) {
    printf("Could not reset ads1219\n");
    return -1;
  }

  while (1) {
    // reset ipc flag
    done = false;
    
    // read measurement
    double voltage = 0.0;
    ret = ads1219_voltage(&voltage);
    if (ret < 0) {
      printf("Could not read ads1219. Retrying\n");
      continue;
    }

    // encode measurement
    size_t core_buf_len = 0;
    Metadata meta = {};
    EncodeDoubleMeasurement(meta, voltage, SensorType_POWER_VOLTAGE, core_buf, &core_buf_len);

    // Send to core for upload
    ipc_notify_service(core_service);
    yield_for(&done);

    // wait 5s after upload
    libtocksync_alarm_delay_ms(5000);
  }

  return 0;
}
