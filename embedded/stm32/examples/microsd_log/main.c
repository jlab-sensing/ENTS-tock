/**
 * @file main.c
 * @brief Routes microlog output to the microSD card attached to the esp32
 *
 * Registers a microlog output handler that forwards every formatted log line
 * over i2c to the esp32, which appends it to /stm32.log on the microSD card.
 * Log lines continue to go to the Tock console as well, the stdout output is
 * never removed.
 */

#include <libents/controller/controller.h>
#include <libents/controller/modules/microsd.h>
#include <libtock-sync/services/alarm.h>
#include <stdbool.h>
#include <ulog.h>

/** Must match the MicroSDCommand.log field, which is 240 bytes (239 chars plus
 * the NUL). Anything smaller truncates the line here, before the proto sees it,
 * and ulog_event_to_cstr() writes "LEVEL FILE:LINE: MESSAGE" into this buffer,
 * so a long __FILE__ eats into the space left for the message. */
#define LOG_LINE_SIZE 240

/**
 * @brief microlog output handler that forwards a log line to the esp32.
 *
 * @param ev Event to format.
 * @param arg Unused user argument from ulog_output_add().
 */
static void microsd_log_output(ulog_event* ev, void* arg) {
  (void)arg;

  // Guard against recursion. If anything reached from ControllerMicroSDLog()
  // ever logs, that log would re-enter this handler and recurse forever.
  static bool busy = false;
  if (busy) {
    return;
  }
  busy = true;

  // static to keep 128 bytes off the stack, matching microlog's own examples.
  // Safe despite being shared state because the busy guard above prevents
  // re-entry.
  static char line[LOG_LINE_SIZE];
  if (ulog_event_to_cstr(ev, line, sizeof(line)) == ULOG_STATUS_OK) {
    ControllerMicroSDLog(line);
  }

  busy = false;
}

int main(void) {
  ulog_info("=== microSD log example ===");

  // Allocates the tx/rx buffers used by ControllerMicroSDLog(). Must happen
  // before the output is registered, otherwise the first forwarded line
  // encodes into a NULL buffer.
  ControllerInit();

  // Give the esp32 a moment to be ready to accept i2c traffic.
  libtocksync_alarm_delay_ms(100);

  ulog_output_id sd_output =
      ulog_output_add(microsd_log_output, NULL, ULOG_LEVEL_INFO);
  if (sd_output == ULOG_OUTPUT_INVALID) {
    // Almost always means ULOG_BUILD_EXTRA_OUTPUTS was not set when microlog
    // was compiled, see embedded/external/microlog/Makefile.
    ulog_error("Could not register microSD log output");
    return 1;
  }

  // This one goes to the console AND to /stm32.log on the card.
  ulog_info("hello world");

  ControllerDeinit();

  return 0;
}
