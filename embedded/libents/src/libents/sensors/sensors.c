/**
 * @file sensors.c
 * @author John Madden (jmadden173@pm.me)
 * @brief See sensors.h
 * @date 2024-04-01
 *
 * @copyright
 *
 * MIT License
 *
 * Copyright (c) 2024 jLab in Smart Sensing at UCSC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "sensors.h"

#include <libtock/kernel/ipc.h>
#include <libtock/services/alarm.h>
#include <stdint.h>
#include <string.h>

#include "../util/time.h"

#ifdef SAVE_TO_MICROSD
#include "controller/microsd.h"
#endif

/** Array for holding function callbacks */
static SensorsPrototypeMeasure callback_arr[MAX_SENSORS];

/** Length of @ref callback_arr */
static unsigned int callback_arr_len = 0;

static const uint8_t kBufferSize = 222;

static libtock_alarm_t sensor_alarm = {};

/** Measurement index counter */
static uint32_t meas_idx = 1;

// setup ipc for uplaods
static int core_service = 0;

// buffer to share with core service
char core_buf[256] __attribute__((aligned(256)));

// flag when ipc is don
static bool done = false;

/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(__attribute__((unused)) int pid, int len, int buf,
                         void* ud) {
  done = true;
}

/**
 * @brief Measures sensors and adds to tx buffer
 *
 * @see libtock_alarm_callback
 */
void SensorsMeasure(uint32_t arg1, uint32_t arg2, void* arg3);

void SensorsStart(int service, uint32_t period_ms) {
  // return codes
  int ret = 0;

  ipc_register_client_callback(core_service, ipc_callback, NULL);
  ipc_share(core_service, core_buf, 256);

  // setup repeating measurement call
  libtock_alarm_repeating_every_ms(period_ms, SensorsMeasure, NULL,
                                   &sensor_alarm);

  // Take first measurement
  SensorsMeasure(0, 0, 0);
}

void SensorsStop(void) { libtock_alarm_ms_cancel(&sensor_alarm); }

int SensorsAdd(SensorsPrototypeMeasure cb) {
  // check for out of range error
  if (callback_arr_len >= MAX_SENSORS) {
    return -1;
  }

  // store callback in array
  callback_arr[callback_arr_len] = cb;

  // return index and increment
  return callback_arr_len++;
}

void SensorsMeasure(uint32_t arg1, uint32_t arg2, void* arg3) {
  // buffer to store measurements
  uint8_t buffer_len;

  // get timestamp
  uint32_t ts = epoch();

  // loop over callbacks
  for (int i = 0; i < callback_arr_len; i++) {
    // call measurement function
    // store length as first byte in buffer
    core_buf[0] = callback_arr[i](core_buf + 1, ts, meas_idx++);

    // skip if error happened
    if (core_buf[0] == -1) {
      continue;
    }

#ifdef SAVE_TO_MICROSD
    ControllerMicroSDSave(buffer, buffer_len);
#endif

    // Send to core for upload
    ipc_notify_service(core_service);
    yield_for(&done);
  }
}

int SensorsAddMeasurement(uint8_t* data, uint8_t data_len) {
  // copy data
  core_buf[0] = data_len;
  memcpy(core_buf + 1, data, data_len);

  // Send to core for upload
  ipc_notify_service(core_service);
  yield_for(&done);
}

uint8_t SensorsMeasureTest(uint8_t* data) {
  uint8_t static_data[] = {0xa,  0xc,  0x8,  0xc8, 0x1,  0x10, 0xc8, 0x1,  0x18,
                           0x88, 0xba, 0xf3, 0xba, 0x6,  0x12, 0x9,  0x11, 0xd9,
                           0xce, 0xf7, 0x53, 0x3,  0x88, 0xb7, 0xc0};

  memcpy(data, static_data, sizeof(static_data));
  return sizeof(static_data);
}
