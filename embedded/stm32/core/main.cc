/* vim: set sw=2 expandtab tw=80: */

#include <stdio.h>

#include <libtock/tock.h>
#include <libtock/kernel/ipc.h>

#include <ulog.h>

#include <libents/storage/fifo.h>
#include <libents/proto/sensor.h>
#include <libents/controller/controller.h>
#include <libents/user_config.h>

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


static bool callback_event = false;


// buffer to store measurements
static uint8_t meas_buffer[256] = {};
static uint8_t meas_buffer_length = 0;

// buffer for user config
static uint8_t uc_buffer[256] = {};
static uint8_t uc_buffer_length = 0;

static uint8_t cmd = 0;;



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
  // Setup logging level and prefix
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("=== App Initialized ===");





  // Load bytes into userconfig buffer
  //
  // Yes I am casting uint8_t to a uint16_t and it could overwrite the data
  // buffer. I know at the time of writing this that the user config stays
  // under 256 bytes as defined by protobuf.
  UserConfigStatus uc_status = UserConfigBytes(uc_buffer, (uint16_t *) &uc_buffer_length);

  printf("uc_buffer[%u]:", uc_buffer_length);
  for (uint8_t i = 0; i < uc_buffer_length; i++) {
    printf(" %02x", uc_buffer[i]);
  }
  printf("\n\n");





  // start service after connected
  ipc_register_service_callback("org.ents.core", ipc_callback, NULL);


  // Print warning when using TEST_USER_CONFIG
#ifdef TEST_USER_CONFIG
  ulog_warn("TEST_USER_CONFIG is enabled!\n");
#endif  // TEST_USER_CONFIG

  // Initialize controller interface
  ControllerInit();

 

  //UserConfigStatus uc_status = UserConfigLoad();
  //// start user config interface
  //if (uc_status == USERCONFIG_OK) {
  //  // print current user config
  //  ulog_info("Current user configuration:");
  //  ulog_info("---------------------------");
  //  UserConfigPrint();
  //} else {
  //  ulog_error("Could not load user config.");
  //}
  




  // Load user config and start webservice with timeotu
  UserConfigStart(120 * 1000);



  // return codes
  int ret = 0;



  // Initialize LoRaWAN

  ret = lorawan_init();
  if (ret < 0) {
    return ret;
  }

  ret = lorawan_join();
  if (ret < 0) {
    return ret;
  }

  ret = lorawan_timesync();
  if (ret < 0) {
    return ret;
  }

  while (1) {
    // TODO: Create copy of counters
    

    // wait for callback
    //yield_for(&callback_event);
    yield();



    //
    // Save data if second command
    // 

    if (cmd == 2) {
      ulog_info("[d] get meas");
      // print out bytes
      //  Get number of bytes in buffer
      ulog_info("Received %d bytes:", meas_buffer_length);
      for (int i=1; i < meas_buffer_length; i++) {
        printf("%x ", meas_buffer[i]);
      }
      printf("\n");

      // store in buffer
      int ret = fifo_put(meas_buffer, meas_buffer_length);
      if (ret < 0) {
        ulog_error("Could not store measurement in buffer");
      }
      stats.meas++;
    }


    //
    // Always check buffer for an upload
    //


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
  }
}

static void ipc_callback(int pid, int len, int buf, void* ud) {
  ulog_trace("ipc_callabck");

  uint8_t* buffer = (uint8_t*) buf;

  // payload format
  cmd = buffer[0];
  uint8_t* length = buffer + 1;
  uint8_t* data = buffer + 2;

  // Reply with userconfig when requested
  if (cmd == 1) {
    ulog_info("[d] get user config");

    memcpy(data, uc_buffer, uc_buffer_length);
    *length = uc_buffer_length;

  // Store measurements into buffer
  } else if (cmd == 2) {
    // copy data to buffer
    meas_buffer_length = *length;
    memcpy(meas_buffer, data, *length);

    // reply with response
    buffer[0] = 0xb;
    buffer[1] = 0xe;
    buffer[2] = 0xe;
    buffer[3] = 0xf;
  // Catch all other commands
  } else {
    ulog_error("IPC Command %d not implemented.", buffer[0]);
  }

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
