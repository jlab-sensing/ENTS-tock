#include <stdio.h>
#include <stdbool.h>

#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>

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
  printf("Sensor App Starting\n");

  int ret = 0;


  int core_service = 0;
  
  ret = ipc_discover("org.ents.core", &core_service);
  if (ret < 0) {
    printf("No core service\n");
    return -1;
  }

  ipc_register_client_callback(core_service, ipc_callback, NULL);
  ipc_share(core_service, core_buf, 256);

  int counter = 0;

  while (1) {
    done = false;
    core_buf[0] = counter++;
    ipc_notify_service(core_service);
    yield_for(&done);

    libtocksync_alarm_delay_ms(1000);
  }


  return 0;
}
