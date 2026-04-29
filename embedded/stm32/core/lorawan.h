#pragma once

#include <stdint.h>


int lorawan_init(void);

int lorawan_join(void);

int lorawan_timesync(void);

int lorawan_upload(uint8_t* buffer, int length);
