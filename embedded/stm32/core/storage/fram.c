#include "fram.h"

#include "fram_def.h"

#define FRAM_MB85RC1MT

#if defined(FRAM_FM24CL16B) && defined(FRAM_MB85RC1MT)
#error Only one FRAM chip can be enabled
#elif defined(FRAM_FM24CL16B)
#error FM24CL16B Not Implemented
#elif defined(FRAM_MB85RC1MT)
#include "mb85rc1mt.h"
const fram_interface_t fram_interface = {.write_ptr = mb85rc1mt_write,
                                         .read_ptr = mb85rc1mt_read,
                                         .size_ptr = mb85rc1mt_size};
#else
#error No FRAM chip enabled
#endif



fram_status fram_write(fram_addr addr, const uint8_t *data, size_t len) {
  if (addr + len > fram_size()) {
      return FRAM_OUT_OF_RANGE;
}

  return fram_interface.write_ptr(addr, data, len);
}

fram_status fram_read(fram_addr addr, size_t len, uint8_t *data) {
  if (addr + len > fram_size()) {
      return FRAM_OUT_OF_RANGE;
  }

  return fram_interface.read_ptr(addr, len, data);
}

fram_addr fram_size(void) {
    return fram_interface.size_ptr();
}
