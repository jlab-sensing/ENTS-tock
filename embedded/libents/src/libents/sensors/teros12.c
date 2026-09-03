#include "teros12.h"

#include <stdlib.h>
#include <math.h>

#include <libtock-sync/peripherals/sdi12.h>
#include <libtock-sync/services/alarm.h>

#include "../proto/sensor.h"


enum {
  BUFFER_SIZE = 32
};


static Teros12Data teros12_data = {};


/**
 * @brief Parse measurement string from Teros21 sensor
 *
 * The buffer is expected to be the following format. [+-] can either be a + or
 * - sign.:
 * a+<calibratedCountsVWC>[+-]<temperature>+<electricalConductivity>
 *
 * Example real world values:
 * 0+1846.16+22.3+1
 *
 * @param buffer Raw measurement string
 * @param data Pointer to the data structure to store the measurement
 * @return SDI12Status
 */
int Teros12ParseMeasurement(const char *buffer);


/**
 * @brief Parses a string
 */
static double parse_double(const char* str) {
  // buffer for storing "integer string"
  char buffer[BUFFER_SIZE] = {};
 
  int decimal_points = 0;

  // Copy without .
  int j = 0;
  for (int i = 0; str[i] != '\0'; i++) {
    if (str[i] != '.') {
      buffer[j++] = str[i];
      // once decimal detected add until end
      if (decimal_points > 0) {
        decimal_points++;
      }
    } else {
      // decimal detected
      decimal_points++;
    }
  }

  // remove the decimal point itself
  decimal_points--;

  // make cstring
  buffer[j] = '\0';

  int num_int = atoi(buffer);
  double num_double = (double) num_int;
  num_double /= pow(10, decimal_points);

  return num_double;
}


int Teros12ParseMeasurement(const char *buffer) {

  // find start of measurement
  char* addr = strpbrk(buffer, "!") + 1;
  if (addr == NULL) {
    return -1;
  }
  teros12_data.addr = *addr;


  // find vwc 
  char* vwc_start = addr + 2;
  char* vwc_end = strpbrk(vwc_start, "+-") - 1;
  if (vwc_end == NULL) {
    return -1;
  }
  const int vwc_str_size = 8;
  char vwc_str[vwc_str_size] = {};
  strncpy(vwc_str, vwc_start, vwc_end - vwc_start + 1);
  teros12_data.vwc = parse_double(vwc_str);
  

  // find temperature (with +/-)
  char* temp_start = vwc_end + 1;
  char* temp_end = strpbrk(temp_start + 1, "+") - 1;
  if (temp_end == NULL) {
    return -1;
  }
  const int temp_str_size = 6;
  char temp_str[temp_str_size] = {};
  strncpy(temp_str, temp_start, temp_end - temp_start + 1);
  teros12_data.temp = parse_double(temp_str);

  // find ec
  char* ec_start = temp_end + 2;
  char* ec_end = strpbrk(ec_start, "\r\n") - 1;
  if (ec_end == NULL) {
    return -1;
  }
  char ec_str[4] = {};
  strncpy(ec_str, ec_start, ec_end - ec_start);
  teros12_data.ec = (unsigned int) atoi(ec_str);

  return 0;
}


Teros12Data Teros12GetMeasurement(void) {
  return teros12_data;
}



int Teros12Measure(char addr) {
  int ret = 0;

  uint8_t buffer[BUFFER_SIZE] = {};

  //
  // Send measure command and wait for service request
  //
  // The read length is hardcoded to the length of the BREAK, Command, and
  // response length.
  //
  // \00M!00013\r\n0\r\n
  //

  const int meas_resp_len = 14;

  uint8_t meas_cmd[] = "0M!";
  meas_cmd[0] = (uint8_t) addr;


  memset(buffer, 0, BUFFER_SIZE);
  ret = libtocksync_sdi12_write_and_receive(meas_cmd, 3, buffer, meas_resp_len);
  if (ret != RETURNCODE_SUCCESS) {
    return -1;
  }

  // clear parity bit
  for (int i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] &= 0x7F;
  }

  //
  // Read measurement data
  // The read length is hardcoded to the length of the BREAK, Command, and
  // response length.
  //
  // \00D0!0+1837.02+19.1+0\r\n
  //
  //

  const int read_resp_len = 23;

  uint8_t read_cmd[] = "0D0!";
  meas_cmd[0] = (uint8_t) addr;
  
  memset(buffer, 0, BUFFER_SIZE);
  ret = libtocksync_sdi12_write_and_receive(read_cmd, 4, buffer, read_resp_len);
  if (ret != RETURNCODE_SUCCESS) {
    return -1;
  }

  // clear parity bit
  for (int i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] &= 0x7F;
  }

  // parse measurement into data structure
  ret = Teros12ParseMeasurement((char *) buffer + 1);

  return ret;
}


uint8_t Teros12MeasureVWC(uint8_t* data, Metadata meta, uint32_t idx) {
  (void) idx;

  size_t data_len = 0;

  // calibration equation for mineral soils from Teros12 user manual and scale
  // to percent scale
  // https://publications.metergroup.com/Manuals/20587_TEROS11-12_Manual_Web.pdf?_gl=1*174xdyp*_gcl_au*MTIxODkwMzcuMTc0MTIwMjU3Nw..
  double vwc_adj = (3.879e-4 * teros12_data.vwc) - 0.6956;
  vwc_adj *= 100;

  SensorStatus status = SENSOR_OK;
  status = EncodeDoubleMeasurement(meta, vwc_adj, SensorType_TEROS12_VWC_ADJ, data, &data_len);
  if (status != SENSOR_OK) {
    return -1;
  }

  return data_len;
}


uint8_t Teros12MeasureVWCRaw(uint8_t* data, Metadata meta, uint32_t idx) {
  (void) idx;
  
  size_t data_len = 0;

  SensorStatus status = SENSOR_OK;
  status = EncodeDoubleMeasurement(meta, teros12_data.vwc, SensorType_TEROS12_VWC, data, &data_len);
  if (status != SENSOR_OK) {
    return -1;
  }

  return data_len;
}

uint8_t Teros12MeasureTemp(uint8_t* data, Metadata meta, uint32_t idx) {
  (void) idx;
  
  size_t data_len = 0;

  SensorStatus status = SENSOR_OK;
  status = EncodeDoubleMeasurement(meta, teros12_data.temp, SensorType_TEROS12_TEMP, data, &data_len);
  if (status != SENSOR_OK) {
    return -1;
  }

  return data_len;
}


uint8_t Teros12MasureEC(uint8_t* data, Metadata meta, uint32_t idx) {
  (void) idx;
  
  size_t data_len = 0;

  SensorStatus status = SENSOR_OK;
  status = EncodeDoubleMeasurement(meta, teros12_data.ec, SensorType_TEROS12_EC, data, &data_len);
  if (status != SENSOR_OK) {
    return -1;
  }

  return data_len;
}
