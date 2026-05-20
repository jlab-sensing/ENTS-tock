/* vim: set sw=2 expandtab tw=80: */

#include <stdio.h>

#include <libtock/tock.h>
#include <libtock/kernel/ipc.h>

#include <ulog.h>

#include <libents/storage/fifo.h>
#include <libents/proto/sensor.h>

#include "lorawan.h"
#include "user_config.h"

/** Stats for uploads */
typedef struct {
  /** Total number of bytes */
  int bytes;
  /** Total number uploads */
  int total;
  /** Failed uploads */
  int failed;
  /** Total number of measurements */
  int meas;
} upload_stats;


upload_stats stats = {};


/**
 * @brief Callback when receiving data for upload from individual apps.
 *
 * @param pid An identifier for the app that notified us.
 * @param len How long the buffer is that the client shared with us.
 * @param buf Pointer to the shared buffer.
 */
static void ipc_callback(int pid, int len, int buf, void* ud);



/**
 * @brief Gets a formatted sensor measurement payload.
 *
 *
 * Peeks into the fram circular buffer and decodes Measurements until the size
 * of a RepeatedSensorMeasurements exceeds the buffer size. Then it encodes
 * RepeatedSensorMeasurements with (n-1) measurements and returns that back.
 *
 * @param buffer Pointer to buffer.
 * @param sizes Size of buffer.
 * @return len Number of bytes in buffer.
 */
static int get_payload(uint8_t* buffer, int size);



void ulog_prefix_handler(ulog_event *ev, char *prefix, size_t prefix_size) {
  snprintf(prefix, prefix_size, "Core\t");
}



int main(void) {
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);

  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("App Initialized\n");



  // Print warning when using TEST_USER_CONFIG
#ifdef TEST_USER_CONFIG
  ulog_warn(TS_OFF, VLEVEL_M, "WARNING: TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG


  UserConfigStart(120);


  // return codes
  int ret = 0;

  // start service after connected
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL); 

  ret = lorawan_init();
  ret = lorawan_join();
  ret = lorawan_timesync();

  while (1) {
    // TODO: Create copy of counters

    ulog_debug("Buffer has %d measurements", fifo_buffer_len()); 

    if (fifo_buffer_len() > 0) {
      // format payload
      uint8_t buffer[256] = {};
      int len = get_payload(buffer, sizeof(buffer));
      if (len == 0) {
        continue;
      }
      ulog_debug("Got payload of %d bytes", len); 

      stats.total++;
      ret = lorawan_upload(buffer, len);
      if (ret < 0) {
        stats.failed++;
        ulog_error("Could not upload with LoRaWAN (error: %d)", ret);
        continue;
      }
      ulog_debug("Uploaded %d bytes with LoRaWAN.");
      
      stats.bytes += len;
    }

    // print stats
    if (stats.total % 10) {
      ulog_info("total uploads: %d\tfailed uploads: %d\tmeasurements: %d\tbytes: %d\t", stats.total, stats.failed, stats.meas, stats.bytes);
    }
      
    yield();
  }
}

static void ipc_callback(int pid, int len, int buf, void* ud) {
  ulog_trace("ipc_callabck");

  uint8_t* buffer = (uint8_t*) buf;

  // TODO: store in circular buffer.

  // print out bytes
  //  Get number of bytes in buffer
  uint8_t buffer_len = buffer[0];
  ulog_info("Received %d bytes:", buffer_len);
  for (int i=1; i < buffer_len; i++) {
    printf("%x ", buffer[i]);
  }
  printf("\n");

  // store in buffer
  int ret = fifo_put(&buffer[1], buffer_len);
  if (ret < 0) {
    ulog_error("Could not store measurement in buffer");
  }
  stats.meas++;


  //ulog_debug("buffer has %d measurements", fifo_buffer_len());

  // reply with response
  buffer[0] = 0xb;
  buffer[1] = 0xe;
  buffer[2] = 0xe;
  buffer[3] = 0xf;

  ipc_notify_client(pid);
}



static int get_payload(uint8_t* buffer, int size) {
  // return codes
  int ret = 0;

  int len = 0;

  Metadata meta = {};
  SensorMeasurement meas[8] = {};

  uint16_t i = 0;
  for (i = 0; i < fifo_buffer_len(); i++) {
    ulog_debug("Buffer idx %d", i);
    ret = fifo_peek(i, buffer, (uint8_t*) &len);
    if (ret < 0) {
      ulog_error("Could not read from buffer (error: %d)", ret);
      continue;
    }
    ulog_debug("Read %d bytes from buffer", len);


    // decode measuremnet
    ret = DecodeSensorMeasurement(buffer, len, &meas[i]);
    if (ret < 0) {
      ulog_error("Could not decode measurement (error %d), malformed data?", ret);
      // TODO: Removed failing measurement. Can't use drop, need to remove
      // specific index.
      continue;
    }

    ret = RepeatedSensorMeasurementsSize(meta, meas, i+1, (size_t*) &len);
    if (ret < 0) {
      ulog_error("Could not find size of payload (error %d)", ret);
      continue;
    }
    ulog_debug("Size of repeated sensor measurements: %d", len);

    // early stop when length exceeds size of buffer
    if (len > size) {
      i--;
      break;
    }
  }
      
  ret = EncodeRepeatedSensorMeasurements(meta, meas, i+1, buffer, sizeof(buffer), (size_t*) &len);
  if (ret < 0) {
    ulog_error("Could encode %d repeated measurements (error %d)", i, ret);
    return 0;
  }

  ret = fifo_drop();
  if (ret < 0) {
    ulog_error("Could not remove measurements from buffer");
  }
  
  return len;
}
