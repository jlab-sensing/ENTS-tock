/**
 * @file hardcoded_user_config.h
 * @brief User config used when building with HARDCODED_USER_CONFIG=1
 *
 * When enabled, this config is written to FRAM on every boot and the config
 * from the webserver is ignored. Without HARDCODED_USER_CONFIG this file is not
 * included.
 *
 * Edit the values below for your device. Fields must stay in the same order as
 * UserConfiguration in libents/proto/user_config.pb.h. Avoid committing real
 * WiFi passwords.
 */

#pragma once

#include <libents/user_config.h>

static const UserConfiguration hardcoded_user_config = {
    .logger_id = 200,
    .cell_id = 200,
    // Uploadmethod_LoRa or Uploadmethod_WiFi
    .Upload_method = Uploadmethod_LoRa,
    // seconds between measurements
    .Upload_interval = 60,
    .enabled_sensors_count = 2,
    .enabled_sensors = {EnabledSensor_Voltage, EnabledSensor_Current},
    // calibration values are taken from 2.2.3-033
    .Voltage_Slope = -0.00039326,
    .Voltage_Offset = 4.92916378e-05,
    .Current_Slope = -1.18693164e-10,
    .Current_Offset = 4.14518594e-05,
    // only used with Uploadmethod_WiFi
    .WiFi_SSID = "",
    .WiFi_Password = "",
    // include the port in the url if it is not 80
    .API_Endpoint_URL = "http://dirtviz.jlab.ucsc.edu/api/sensor/",
    // deprecated, the port is taken from the url
    .API_Endpoint_Port = 80,
};
