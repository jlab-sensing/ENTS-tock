/**
 * @file alarm_syscalls.h
 * @brief Host stub for the Tock alarm syscalls.
 *
 * Only the two entry points uptime.c uses. The test binary provides the
 * definitions, so a test can drive the tick counter and the reported frequency
 * directly.
 */

#pragma once

#include <stdint.h>

int libtock_alarm_command_get_frequency(uint32_t* frequency);
int libtock_alarm_command_read(uint32_t* time);
