#include <libtock-sync/services/alarm.h>
#include <libtock-sync/peripherals/gpio.h>
#include <stdio.h>
#include <ulog.h>

static const uint8_t esp32_wakeup_pin = 1;

static const uint8_t esp32_reset_pin = 6;

int main(void) {
  // Print using provided logging library
  ulog_info("Template Example");

  // print "hello world!" every 2 seconds
  // NOTE: The \n is required to flush the output buffer and see the print
  // statements in the console.
  while (1) {
    libtock_gpio_set(esp32_wakeup_pin);
  }

  return 0;
}
