/* vim: set sw=2 expandtab tw=80: */

#include <stdio.h>

#include <libtock/tock.h>
#include <libtock/kernel/ipc.h>

#include "lorawan.h"

static int counter = 0;


/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(int pid, int len, int buf, void* ud);




int main(void) {
  printf("ENTS Core\n");
 
  // service
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL);

  while (1) {
    yield();
  }
}





static void ipc_callback(int pid, int len, int buf, void* ud) {
  uint8_t* buffer = (uint8_t*) buf;

  // TODO: store in circular buffer.

  // print out bytes
  for (int i=0; i < len; i++) {
    printf("%x", buffer[i]);
  }
  printf("\n");

  // reply with response
  buffer[0] = 0xb;
  buffer[1] = 0xe;
  buffer[2] = 0xe;
  buffer[3] = 0xf;

  ipc_notify_client(pid);
}
