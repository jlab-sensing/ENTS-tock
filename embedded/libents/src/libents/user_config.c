#include "user_config.h"

#include <stdio.h>

#include "./proto/transcoder.h"
#include "./storage/fram.h"

// Static variable to store the loaded user configuration in RAM
static UserConfiguration loadedConfig = {};

#ifdef TEST_USER_CONFIG
const static UserConfiguration testConfig = {
    .logger_id = 200,
    .cell_id = 200,
    .Upload_method = Uploadmethod_WiFi,
    .Upload_interval = 10,
    .enabled_sensors_count = 2,
    .enabled_sensors = {EnabledSensor_Voltage, EnabledSensor_Current},
    // calibration values are taken from 2.2.3-033
    .Voltage_Slope = -0.00039326,
    .Voltage_Offset = 4.92916378e-05,
    .Current_Slope = -1.18693164e-10,
    .Current_Offset = 4.14518594e-05,
    .WiFi_SSID = "HARE_Lab",
    .WiFi_Password = "",
    .API_Endpoint_URL = "http://dirtviz.jlab.ucsc.edu/api/sensor/",
    // port is not used
    .API_Endpoint_Port = 80};
#endif  // TEST_USER_CONFIG

/**
 * @brief    Writes user configuration data to FRAM.
 *
 * This function writes a specified amount of user configuration data
 * to the FRAM memory starting at a specified address.
 *
 * @param    addr  Starting address in FRAM to write data.
 * @param    data       Pointer to the data to be written.
 * @param    length     Length of the data to be written.
 * @return   USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfig_WriteToFRAM(uint16_t addr, uint8_t* data,
                                        uint16_t length);

/**
 * @brief Reads user configuration data from FRAM.
 *
 * This function reads a specified amount of user configuration data
 * from the FRAM memory starting at a specified address.
 *
 * @param addr   Starting address in FRAM to read data from.
 * @param length      Length of the data to be read.
 * @param data        Pointer to the buffer to store the read data.
 * @return USERCONFIG_OK if successful, error code otherwise.
 */
UserConfigStatus UserConfig_ReadFromFRAM(uint16_t addr, uint16_t length,
                                         uint8_t* data);

UserConfigStatus UserConfig_WriteToFRAM(uint16_t addr, uint8_t* data,
                                        uint16_t length) {
  fram_status status = fram_write(addr, data, length);
  if (status != FRAM_OK) {
    return USERCONFIG_FRAM_ERROR;
  }

  return USERCONFIG_OK;
}

UserConfigStatus UserConfig_ReadFromFRAM(uint16_t addr, uint16_t length,
                                         uint8_t* data) {
  fram_status status = fram_read(addr, length, data);
  printf("\n\n[d] Read from FRAM status: %d\n\n", status);
  if (status != FRAM_OK) {
    return USERCONFIG_FRAM_ERROR;
  }

  return USERCONFIG_OK;
}

UserConfigStatus UserConfigBytes(uint8_t* buffer, uint16_t* length) {
#ifdef TEST_USER_CONFIG
  return USERCONFIG_OK;
#else
  uint8_t length_buf[2];

  // Read the length of the user configuration data from FRAM
  if (UserConfig_ReadFromFRAM(USER_CONFIG_LEN_ADDR, 2, length_buf) !=
      USERCONFIG_OK) {
    return USERCONFIG_FRAM_ERROR;
  }

  // Convert length bytes to integer
  *length = (length_buf[0] << 8) | length_buf[1];

  if (*length == 0) {
    return USERCONFIG_EMPTY_CONFIG;
  }

  // check the length for errors
  if (*length > UserConfiguration_size) {
    return USERCONFIG_FRAM_ERROR;
  }

  // Read the encoded configuration data from FRAM into RX_Buffer
  if (UserConfig_ReadFromFRAM(USER_CONFIG_START_ADDRESS, *length, buffer) !=
      USERCONFIG_OK) {
    return USERCONFIG_FRAM_ERROR;
  }

  return USERCONFIG_OK;
#endif  // TEST_USER_CONFIG
}

UserConfigStatus UserConfigLoadBytes(uint8_t* buffer, uint16_t length) {
  // Decode the user configuration from RX_Buffer into loadedConfig struct
  if (DecodeUserConfiguration(buffer, length, &loadedConfig) != USERCONFIG_OK) {
    // Return an error if decoding fails
    return USERCONFIG_DECODE_ERROR;
  }

  return USERCONFIG_OK;
}

// Load user configuration data from FRAM to RAM
UserConfigStatus UserConfigLoad(void) {
#ifdef TEST_USER_CONFIG
  return USERCONFIG_OK;
#else

  uint8_t buffer[UserConfiguration_size] = {};
  uint16_t data_length = 0;

  UserConfigStatus status = USERCONFIG_OK;

  /// get bytes
  status = UserConfigBytes(buffer, &data_length);
  if (status != USERCONFIG_OK) {
    return status;
  }

  // Decode the user configuration from RX_Buffer into loadedConfig struct
  status = UserConfigLoadBytes(buffer, data_length);
  if (status != USERCONFIG_OK) {
    return status;
  }

  return USERCONFIG_OK;  // Return success if decoding is successful
#endif  // TEST_USER_CONFIG
}

// Get a reference to the loaded user configuration data in RAM.
const UserConfiguration* UserConfigGet(void) {
#ifdef TEST_USER_CONFIG
  return &testConfig;
#else
  return &loadedConfig;
#endif  // TEST_USER_CONFIG
}

UserConfigStatus UserConfigSave(const UserConfiguration* config) {
  if (config == NULL) {
    return USERCONFIG_NULL_CONFIG;
  }

  uint8_t encoded_data[UserConfiguration_size];
  size_t encoded_length = EncodeUserConfiguration(config, encoded_data);
  if (encoded_length == -1) {
    return USERCONFIG_ENCODE_ERROR;
  }

  // Write the length of the encoded data to FRAM
  uint8_t length_buf[2] = {(encoded_length >> 8) & 0xFF, encoded_length & 0xFF};
  UserConfigStatus status =
      UserConfig_WriteToFRAM(USER_CONFIG_LEN_ADDR, length_buf, 2);
  if (status != USERCONFIG_OK) {
    return status;
  }

  // Write the encoded data to FRAM
  status = UserConfig_WriteToFRAM(USER_CONFIG_START_ADDRESS, encoded_data,
                                  encoded_length);
  if (status != USERCONFIG_OK) {
    return status;
  }

  return USERCONFIG_OK;
}

void UserConfigPrintAny(const UserConfiguration* config) {
  // Print each member of the UserConfiguration
  printf("Logger ID: %u\r\n", config->logger_id);
  printf("Cell ID: %u\r\n", config->cell_id);

  if (config->Upload_method == 0) {
    printf("Upload Method: %u \"LoRa\"\r\n", config->Upload_method);
  } else {
    printf("Upload Method: %u \"WiFi\"\r\n", config->Upload_method);
  }

  printf("Upload Interval: %u\r\n", config->Upload_interval);

  for (int i = 0; i < config->enabled_sensors_count; i++) {
    const char* sensor_name = NULL;
    switch (config->enabled_sensors[i]) {
      case EnabledSensor_Voltage:
        sensor_name = "Voltage";
        break;
      case EnabledSensor_Current:
        sensor_name = "Current";
        break;
      case EnabledSensor_Teros12:
        sensor_name = "Teros12";
        break;
      case EnabledSensor_Teros21:
        sensor_name = "Teros21";
        break;
      case EnabledSensor_BME280:
        sensor_name = "BME280";
        break;
      case EnabledSensor_Phytos31:
        sensor_name = "Phytos31";
        break;
      case EnabledSensor_SEN0308:
        sensor_name = "SEN0308";
        break;
      case EnabledSensor_SEN0257:
        sensor_name = "SEN0257";
        break;
      case EnabledSensor_YFS210C:
        sensor_name = "YFS210C";
        break;
      case EnabledSensor_PCAP02:
        sensor_name = "PCAP02";
        break;
      default:
        sensor_name = "Unknown Sensor";
    }
    printf("Enabled Sensor %d: %s\r\n", i + 1, sensor_name);
  }

  char float_str[100];

  snprintf(float_str, sizeof(float_str), "%e", config->Voltage_Slope);
  printf("Calibration V Slope: %s\r\n", float_str);

  snprintf(float_str, sizeof(float_str), "%e", config->Voltage_Offset);
  printf("Calibration V Offset: %s\r\n", float_str);

  snprintf(float_str, sizeof(float_str), "%e", config->Current_Slope);
  printf("Calibration I Slope: %s\r\n", float_str);

  snprintf(float_str, sizeof(float_str), "%e", config->Current_Offset);
  printf("Calibration I Offset: %s\r\n", float_str);

  printf("WiFi SSID: %s\r\n", config->WiFi_SSID);

  printf("WiFi Password: %s\r\n", config->WiFi_Password);

  printf("API Endpoint URL: %s\r\n", config->API_Endpoint_URL);

  printf("API Port: %u\r\n", config->API_Endpoint_Port);
}

void UserConfigPrint(void) {
  const UserConfiguration* config = UserConfigGet();

  UserConfigPrintAny(config);
}

UserConfigStatus UserConfigClear(void) {
  fram_status status = FRAM_OK;
  const uint8_t length[2] = {};
  const uint8_t empty[UserConfiguration_size] = {};
  status = fram_write(USER_CONFIG_LEN_ADDR, length, sizeof(length));
  if (status != FRAM_OK) {
    return USERCONFIG_FRAM_ERROR;
  }
  status = fram_write(USER_CONFIG_START_ADDRESS, empty, sizeof(empty));
  if (status != FRAM_OK) {
    return USERCONFIG_FRAM_ERROR;
  }
  return USERCONFIG_OK;
}
