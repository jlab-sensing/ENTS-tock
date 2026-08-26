#include <libents/storage/fram.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main() {
  printf("=== Clear nonces app ===");

  int status = 0;

  uint32_t nonces_addr = 2200;

  const size_t nonces_size = 14;

  uint8_t nonces[nonces_size] = {};

  // Get current value
  status = fram_read(nonces_addr, nonces_size, nonces);
  if (status < 0) {
    printf("Error! Could not read current nonces.\n");
  }
  printf("Current nonces buffer:");
  for (size_t i = 0; i < nonces_size; i++) {
    printf("%02x ", nonces[i]);
  }
  printf("\n\n");

  // Write to buffer
  memset(nonces, 0, nonces_size);
  status = fram_write(nonces_addr, nonces, nonces_size);
  if (status < 0) {
    printf("Error could not write to nonces.\n");
  }

  status = fram_read(nonces_addr, nonces_size, nonces);
  if (status < 0) {
    printf("Error! Could not read current nonces.\n");
  }
  printf("New nonces buffer:");
  for (size_t i = 0; i < nonces_size; i++) {
    printf("%02x ", nonces[i]);
  }
  printf("\n\n");

  printf("New nonces buffer should be all zeros.\n");

  printf("\nDone.\n");

  return 0;
}
