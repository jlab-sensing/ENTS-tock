/* vim: set sw=2 expandtab tw=80: */

#include <stdio.h>

#include <libtock/tock.h>
#include <libtock/kernel/ipc.h>

#include <ulog.h>

#include "lorawan.h"

/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(int pid, int len, int buf, void* ud);



void ulog_prefix_handler(ulog_event *ev, char *prefix, size_t prefix_size) {
  snprintf(prefix, prefix_size, "Core\t");
}



int main(void) {
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);

  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("App Initialized\n");
 
  // service
  //ipc_register_service_callback("org.ents.core", ipc_callback, NULL);


  lorawan_init();
  lorawan_join();
  lorawan_timesync();

  while (1) {
    yield();
  }
}



static void ipc_callback(int pid, int len, int buf, void* ud) {
  int buffer_len = (int) buf;
  uint8_t* buffer = ((uint8_t*) buf) + 1;

  // TODO: store in circular buffer.

  // print out bytes
  ulog_info("Received bytes:");
  for (int i=0; i < len; i++) {
    printf("%x", buffer[i]);
  }
  printf("\n");

  //lorawan_upload(buffer, buffer_len);

  // reply with response
  buffer[0] = 0xb;
  buffer[1] = 0xe;
  buffer[2] = 0xe;
  buffer[3] = 0xf;

  ipc_notify_client(pid);
}
