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



  lorawan_setup();


  while (1) {
    yield();
  }
}





static void ipc_callback(int pid, int len, int buf, void* ud) {
  uint8_t* buffer = (uint8_t*) buf;

  // TODO: store in circular buffer.
  counter = (int) (buffer[0]);

  ipc_notify_client(pid);
}
