#include <libtock-sync/services/alarm.h>
#include <libtock/peripherals/i2c_master.h>
#include <stdint.h>
#include <ulog.h>

int main(void) {
  ulog_info("I2C communication example with esp32");

  // Device address, match to what is in esp32
  const int addr = 0x20 << 1;

  int ret = 0;

  uint8_t data[5] = "hello";
  ret = i2c_master_write_sync(0x20 << 1, data, 5);
  if (ret < 0) {
    ulog_error("Error writing to esp32: %d", ret);
  } else {
    ulog_info("Successfully wrote to esp32");
  }

  libtocksync_alarm_delay_ms(10);

  uint8_t buffer[32] = {};
  ret = i2c_master_read_sync(0x20 << 1, buffer, 5);
  if (ret < 0) {
    ulog_error("Error reading from esp32: %d", ret);
  } else {
    ulog_info("Successfully read from esp32");
    // ulog_info("Data read: %02x", buffer[0]);
    ulog_info("Data read: %c %c %c %c %c", buffer[0], buffer[1], buffer[2],
              buffer[3], buffer[4]);
    ulog_info("Data read: %x %x %x %x %x", buffer[0], buffer[1], buffer[2],
              buffer[3], buffer[4]);
  }

  // libtocksync_alarm_delay_ms(1000);

  // ulog_info("repeat");
  // ret = i2c_master_read_sync(0x20 << 1, buffer, 1);
  // if (ret < 0) {
  //   ulog_error("Error reading from esp32: %d", ret);
  // } else {
  //   ulog_info("Successfully read from esp32");
  //   //ulog_info("Data read: %02x", buffer[0]);
  //   ulog_info("Data read: %c %c %c %c %c", buffer[0], buffer[1], buffer[2],
  //   buffer[3], buffer[4]); ulog_info("Data read: %x %x %x %x %x", buffer[0],
  //   buffer[1], buffer[2], buffer[3], buffer[4]);
  // }

  return 0;
}
