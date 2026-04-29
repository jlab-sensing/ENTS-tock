#include "controller.h"

#include <stdlib.h>

#include <libtock/peripherals/gpio.h>
#include <libtock-sync/services/alarm.h>

#include "communication.h"
#include "../proto/controller.pb.h"


// TODO: Update with correct value.
static const uint8_t esp32_wakeup_pin = 5;


void ControllerInit(void) {
  const size_t buffer_size = Esp32Command_size;

  Buffer *tx = ControllerTx();

  // allocate tx buffer
  tx->data = (uint8_t *)malloc(buffer_size);
  tx->size = buffer_size;
  tx->len = 0;

  Buffer *rx = ControllerRx();

  // allocate rx buffer
  rx->data = (uint8_t *)malloc(buffer_size);
  rx->size = buffer_size;
  rx->len = 0;


  // Enable output pin
  libtock_gpio_enable_output(esp32_wakeup_pin);
}

void ControllerDeinit(void) {
  Buffer *tx = ControllerTx();

  // free tx buffer
  free(tx->data);
  tx->size = 0;
  tx->len = 0;

  Buffer *rx = ControllerRx();

  // free rx buffer
  free(rx->data);
  rx->size = 0;
  rx->len = 0;

  // Turn off to save power
  libtock_gpio_disable(esp32_wakeup_pin);
}

void ControllerWakeup(void) {
  libtock_gpio_set(esp32_wakeup_pin);

  libtocksync_alarm_delay_ms(50);
  
  libtock_gpio_clear(esp32_wakeup_pin);
  
  libtocksync_alarm_delay_ms(50);
}
