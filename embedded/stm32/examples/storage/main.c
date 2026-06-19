#include <libents/storage/fram.h>
#include <stdio.h>

int main(void) {
  printf("Hello World\n");

  uint8_t write_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
  fram_addr addr = 0x100;

  fram_status status = fram_write(addr, write_data, sizeof(write_data));
  printf("fram_write status: %d", status);

  uint8_t read_data[5] = {0};
  status = fram_read(addr, sizeof(read_data), read_data);
  printf("fram_read status: %d", status);

  for (int i = 0; i < 5; i++) {
    printf("%x ", write_data[i]);
  }
  printf("\n");

  for (int i = 0; i < 5; i++) {
    printf("%x ", read_data[i]);
  }
  printf("\n");

  return 0;
}
