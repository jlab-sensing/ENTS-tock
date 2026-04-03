#include <libtock-sync/services/alarm.h>
#include <stdio.h>
#include <ulog.h>

int main(void) {
  // Print using provided logging library
  ulog_info("Template Example");

  // print "hello world!" every 2 seconds
  // NOTE: The \n is required to flush the output buffer and see the print
  // statements in the console.
  while (1) {
    printf("Hello world!\n");
    libtocksync_alarm_delay_ms(2000);
  }

  return 0;
}
