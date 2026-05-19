/**
 * @file    ads1219.c
 * @author  Stephen Taylor
 * @brief   Soil Power Sensor ADS12 library
 * @date    11/27/2023
 *
 * This file provides a function to read from the ADS1219.
 *
 * This file was copied and modified from the ENTS libraries.
 *
 * TODO (jmadden173): Add support for data ready pin
 * TODO (jmadden173): Improve lib with big bang for regs
 * TODO (jmadden173): Add offset calibration at startup
 **/

#include "ads1219.h"

#include <libtock/tock.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/peripherals/i2c_master.h>


// Generic calibration values that give rough estimate
static const double voltage_calibration_m = -0.00039326;
static const double voltage_calibration_b = 4.92916378e-05;
static const double current_calibration_m = -1.18693164e-10;
static const double current_calibration_b = 4.14518594e-05;

static const uint8_t addr = 0x40;
/** i2c address left shifted one bit for hal i2c funcs */
static uint8_t addrls = addr << 1;

/** i2c address */
typedef enum {
  /** Reset the device */
  cmd_reset = 0x06,
  /** Start or restart conversion */
  cmd_start = 0x08,
  /** Enter power-down mode */
  cmd_powerdown = 0x02,
  /** Read data */
  cmd_rdata = 0x10,
  /** Write to register */
  // prev 0x80
  cmd_rreg = 0x20,
  /** Read from register */
  cmd_wreg = 0x40
} ads1219_cmd_t;

/**
 * Control register breakdown.
 *
 * The implementation for the bit field uses the first value as the LSB. Aka
 * vref is the LSB and mux is the MSB.
 *
 *  7:5 MUX (default)
 *  4   Gain (default)
 *  3:2 Data rate (default)
 *  1   Conversion mode (default)
 *  0   VREF (External reference 3.3V)
 */
typedef union {
  uint8_t value;
  struct {
    uint8_t vref : 1;
    uint8_t mode : 1;
    uint8_t dr : 2;
    uint8_t gain : 1;
    uint8_t mux : 3;
  } bits;
} ConfigReg;

/**
 * @brief GPIO port for adc data ready line
 *
 * @see data_ready_pin
 */
//const GPIO_TypeDef *data_ready_port = GPIOC;

/**
 * @brief GPIO pin for adc data ready line
 *
 */
//const uint16_t data_ready_pin = GPIO_PIN_0;

/**
 * @brief Turn on power to analog circuit
 *
 * Has a blocking wait of 1 mS to account for the startup time of OpAmps. This
 * should be an order of magnitude greater than the startup or turn-on time of
 * the opamps used in the circuit.
 *
 * MAX9944 - N/A
 * INA296 - 20 uS
 * THS4532 - 420 nS
 *
 * @see data_ready_pin
 */
void power_on(void);

/**
 * @brief Turn off power to analog circuit
 *
 * @see data_ready_pin
 */
void power_off(void);

/**
 * @brief Measure from the adc
 *
 * @param mwas Raw measurement
 *
 * @return Raw measurement from adc
 */
int measure(uint32_t *meas);

/**
 * @brief This function reconfigures the ADS1219 based on the parameter reg_data
 *
 * @param reg_data
 * @return int
 */
int configure(const ConfigReg reg_data);

int ads1219_reset(void) {
  int ret = RETURNCODE_SUCCESS;

  // Send the reset code
  uint8_t write_buf[1] = {cmd_reset};
  ret = i2c_master_write_sync(addrls, write_buf, 1);
  if (ret != RETURNCODE_SUCCESS) {
    return ADS1219_ERROR;
  }

  // wait minimum 500 us to reach steady state
  libtocksync_alarm_delay_ms(1);

  return ADS1219_SUCCESS;
}

int configure(const ConfigReg reg_data) {
  int ret = RETURNCODE_SUCCESS;
  uint8_t i2c_data[2] = {cmd_wreg, reg_data.value};
  ret = i2c_master_write_sync(addrls, (uint8_t *)i2c_data, sizeof(i2c_data));
  if (ret != RETURNCODE_SUCCESS) {
    return ADS1219_CONFIGURE;
  }
  
  return ADS1219_SUCCESS;
}

int ads1219_voltage_raw(uint32_t *voltage) {
  int ret = ADS1219_SUCCESS;

  ConfigReg reg_data = {0};
  reg_data.bits.vref = 1;

  // 0x21 is single shot and 0x23 is continuos
  // configure to read voltage
  ret = configure(reg_data);
  if (ret != ADS1219_SUCCESS) {
    return ret;
  }

  ret = measure(voltage);
  if (ret != ADS1219_SUCCESS) {
    return ret;
  }

  return ret;
}

int ads1219_voltage(double *voltage) {
  int ret = ADS1219_SUCCESS;

  uint32_t raw = 0;
  ret = ads1219_voltage_raw(&raw);

  *voltage = (voltage_calibration_m * (double) raw) + voltage_calibration_b;
  *voltage /= 1000;

  return ret;
}

int ads1219_current_raw(uint32_t *current) {
  int ret = ADS1219_SUCCESS;
  double meas = 0.0;

  ConfigReg reg_data = {0};
  reg_data.bits.mux = 0b001;
  reg_data.bits.vref = 1;

  // 0x21 is single shot and 0x23 is continuos
  // configure to read voltage
  ret = configure(reg_data);
  if (ret != ADS1219_SUCCESS) {
    return -1;
  }

  ret = measure(current);
  if (ret != ADS1219_SUCCESS) {
    return -1;
  }

  return meas;
}

int ads1219_current(double *current) {
  int ret = ADS1219_SUCCESS;

  uint32_t raw = 0;
  ret = ads1219_current_raw(&raw);

  *current = (current_calibration_m * (double) raw) + current_calibration_b;
  return ret; 
}

void power_on(void) {
  //// set high
  //HAL_GPIO_WritePin(POWERDOWN_GPIO_Port, POWERDOWN_Pin, GPIO_PIN_SET);
  //// delay for settling of analog components
  //HAL_Delay(1);
}

void power_off(void) {
  //// set low
  //HAL_GPIO_WritePin(POWERDOWN_GPIO_Port, POWERDOWN_Pin, GPIO_PIN_RESET);
}

int measure(uint32_t *meas) {
  int ret = RETURNCODE_SUCCESS;

  uint8_t rx_data[3] = {0x00, 0x00, 0x00};

  power_on();

  // start conversion
  uint8_t buf[1] = {cmd_start};
  ret = i2c_master_write_sync(addrls, buf, 1);
  if (ret != RETURNCODE_SUCCESS) {
    return ADS1219_CONVERSION;
  }

  // constant delay
  libtocksync_alarm_delay_ms(100);

  // TODO(jmadden173) implement data ready pin wait
  // Wait for the DRDY pin on the ADS12 to go low, this means data is ready
  //while (HAL_GPIO_ReadPin(data_ready_port, data_ready_pin)) {}

  // send read data command
  buf[0] = cmd_rdata;
  ret = i2c_master_write_sync(addrls, buf, 1);
  if (ret != RETURNCODE_SUCCESS) {
    return ADS1219_MEASURE;
  }

  // read 3 bytes of data
  ret = i2c_master_read_sync(addrls, rx_data, 3);
  if (ret != RETURNCODE_SUCCESS) {
    return ADS1219_MEASURE;
  }

  power_off();

  // Combine the 3 bytes into a 24-bit value
  *meas = ((int32_t)rx_data[0] << 16) | ((int32_t)rx_data[1] << 8) |
          ((int32_t)rx_data[2]);
  // Check if the sign bit (24th bit) is set
  if (*meas & 0x800000) {
    // Extend the sign to 32 bits
    *meas |= 0xFF000000;
  }

  return ADS1219_SUCCESS;
}
