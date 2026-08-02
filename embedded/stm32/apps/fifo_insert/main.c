#include <libents/storage/fifo.h>
#include <stdio.h>
#include <ulog.h>

int main(void) {
  const uint8_t payload[] = {16, 32, 48, 64, 80};
  const size_t payload_len = sizeof(payload) / sizeof(payload[0]);

  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_info("FIFO insert test app starting...");

  fram_status status = fifo_init();
  if (status != FRAM_OK) {
    ulog_error("fifo_init failed: %d", status);
    return 1;
  }

  status = fifo_put(payload, payload_len);
  if (status != FRAM_OK) {
    ulog_error("fifo_put failed: %d", status);
    return 1;
  }

  ulog_info("Inserted %zu bytes into FIFO.", payload_len);
  ulog_info("FIFO buffer length: %u", fifo_buffer_len());
  return 0;
}
