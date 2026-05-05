/* vim: set sw=2 expandtab tw=80: */

#include <stdio.h>

#include <libtock/tock.h>
#include <libtock/kernel/ipc.h>

#include <ulog.h>

#include "lorawan.h"


static bool connected = false;


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
  
  // start service after connected
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL); 

  lorawan_init();
  lorawan_join();
  lorawan_timesync();
  
  connected = true;  

  while (1) {
    yield();
  }
}

static void ipc_callback(int pid, int len, int buf, void* ud) {
  ulog_trace("ipc_callabck");
  uint8_t* buffer = (uint8_t*) buf;

  // TODO: store in circular buffer.

  // print out bytes
  //  Get number of bytes in buffer
  uint8_t buffer_len = buffer[0];
  ulog_info("Received %d bytes:", buffer_len);
  for (int i=1; i < buffer_len; i++) {
    printf("%x ", buffer[i]);
  }
  printf("\n");

  if (connected) {
    lorawan_upload(&buffer[1], buffer_len);
  }

  // reply with response
  buffer[0] = 0xb;
  buffer[1] = 0xe;
  buffer[2] = 0xe;
  buffer[3] = 0xf;

  ipc_notify_client(pid);
}
