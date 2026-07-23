#include <libents/sensors/ads1219.h>
#include <libtock-sync/interface/console.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/peripherals/gpio.h>
#include <stdio.h>
#include <ulog.h>

typedef struct {
  // measurement time (s)
  int meas_time;
  // pin to capacitor switch
  int cap_sw;
  // pin to load switch
  int load_sw;
  // time for signal to settle after analog switching (ms)
  int settle_time;
} config;

static const config cfg = {
    .meas_time = 2, .cap_sw = 3, .load_sw = 4, .settle_time = 10};

/**
 * @brief Callback to add prefix to ulog messages.
 *
 * @param ev Pointer to ulog event.
 * @param prefix Pointer to prefix buffer.
 * @param prefix_size Size of prefix buffer.
 */
void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size);

/**
 * @brief Setup necessary peripherials.
 *
 * Resets the ADC to a known state. Clears output on pins to disconnet
 * switches. Switches pins to output mode.
 */
void reset(void);

/**
 * @brief Puts system into a charge state.
 *
 * Connects the capacitor to the MFC terminals. Disconnects load.
 */
void charge(void);

/**
 * @brief Measures current capacitor voltage.
 */
void measure(void);

/**
 * @brief Discharge capacitor across known static load.
 *
 * Disconnects capacitor and MFC terminals. Waits. Connects capacitor to load.
 * Wait.
 */
void discharge(void);

void process_cmd(const uint8_t* cmd);

/**
 * @brief Print the current config.
 */
void print_config(void);

void print_cmd_start(void);

void print_backspace(void);

void ulog_prefix_handler(ulog_event* ev, char* prefix, size_t prefix_size) {
  (void)ev;

  snprintf(prefix, prefix_size, "Dynapex\t");
}

void reset(void) {
  int ret = 0;

  // ads1219
  ret = ads1219_reset();
  if (ret < 0) {
    ulog_error("Could not reset ads1219.");
  }

  // gpio

  // capacitor switch
  ret = libtock_gpio_clear(cfg.cap_sw);
  if (ret < 0) {
    ulog_error("Could not set capacitor switch pin to LOW, %d", ret);
  }
  ret = libtock_gpio_enable_output(cfg.cap_sw);
  if (ret < 0) {
    ulog_error("Could not set capacitor switch pin to output, %d", ret);
  }

  // load switch
  ret = libtock_gpio_clear(cfg.load_sw);
  if (ret < 0) {
    ulog_error("Could not set load switch pin to LOW, %d", ret);
  }
  ret = libtock_gpio_enable_output(cfg.load_sw);
  if (ret < 0) {
    ulog_error("Could not set load switch pin to output, %d", ret);
  }
}

void print_config(void) {
  printf("Measurement time (s): %d\n", cfg.meas_time);
  printf("Capacitor switch pin: %d\n", cfg.cap_sw);
  printf("Load switch pin: %d\n", cfg.load_sw);
  printf("Settle time (ms): %d\n", cfg.settle_time);
}

void charge(void) {
  int ret = 0;

  ret = libtock_gpio_clear(cfg.load_sw);
  if (ret < 0) {
    ulog_error("Could not set load switch to LOW, %d", ret);
  }

  libtocksync_alarm_delay_ms(cfg.settle_time);

  ret = libtock_gpio_set(cfg.cap_sw);
  if (ret < 0) {
    ulog_error("Could not set capacitor switch to HIGH, %d", ret);
  }
}

void discharge(void) {
  int ret = 0;

  ret = libtock_gpio_clear(cfg.cap_sw);
  if (ret < 0) {
    ulog_error("Could not set capacitor switch to HIGH, %d", ret);
  }

  libtocksync_alarm_delay_ms(cfg.settle_time);

  ret = libtock_gpio_set(cfg.load_sw);
  if (ret < 0) {
    ulog_error("Could not set load switch to LOW, %d", ret);
  }

  libtocksync_alarm_delay_ms(cfg.settle_time);
}

void measure(void) {
  int ret = 0;

  // Read from adc
  double voltage = 0.;
  ret = ads1219_voltage(&voltage);
  if (ret < 0) {
    ulog_error("Could not read voltage, %d", ret);
  }

  printf("%07d uV\n", (int)(voltage * 1e6));
}

void print_cmd_start(void) {
  const uint8_t cmd_start[3] = "> ";

  // write back to console
  int write = 0;
  libtocksync_console_write(cmd_start, 2, &write);
}

void print_backspace(void) {
  const uint8_t back[4] = "\b \b";

  // write back to console
  int write = 0;
  libtocksync_console_write(back, 3, &write);
}

void process_cmd(const uint8_t* cmd) {
  char first = cmd[0];

  switch (first) {
    case 'C':
      charge();
      break;
    case 'D':
      discharge();
      break;
    case 'M':
      measure();
      break;
    case 'R':
      reset();
      break;
    case 'W':
      ulog_warn("WAIT is not implemented.");
      break;
    case 'H':
    default:
      printf(
          "Available commands are: [C]HARGE, [D]ISCHARGE, [M]EASURE, [R]ESET, "
          "[W]AIT, [H]ELP\n");
  }
}

int main() {
  // Setup logging level and prefix
  ulog_output_level_set_all(ULOG_LEVEL_TRACE);
  ulog_prefix_set_fn(ulog_prefix_handler);
  ulog_info("=== Dynapex Initialized ===");

  print_config();

  reset();

  print_cmd_start();

  // Read commands
  while (1) {
    int ret = 0;

    static uint8_t buf[16] = {};
    static int idx = 0;

    // printf("buffer:");
    // for (int i = 0; i < sizeof(buf); i++) {
    //   printf(" %x", buf[i]);
    // }
    // printf("\n");

    // check overflow
    if ((size_t)idx >= sizeof(buf)) {
      ulog_error("Input buffer overlow. Resetting buffer.");

      // clear
      memset(buf, 0, sizeof(buf));
      idx = 0;

      continue;
    }

    // read next character
    int read = 0;
    ret = libtocksync_console_read((uint8_t*)buf + idx, 1, &read);
    if (ret < 0) {
      ulog_error("Could not read from console, %d. Resetting buffer.", ret);

      // clear
      memset(buf, 0, sizeof(buf));
      idx = 0;

      continue;
    }

    // look for \n to process command and clear register
    if (buf[idx] == 0xd) {
      printf("\n");

      process_cmd(buf);

      // clear
      memset(buf, 0, sizeof(buf));
      idx = 0;

      print_cmd_start();

      continue;
    }

    // backspace character
    if (buf[idx] == 0x8) {
      // check if at start
      if (idx == 0) {
        continue;
      }

      print_backspace();

      buf[idx] = 0;
      --idx;
      continue;
    }

    // write back to console
    int write = 0;
    libtocksync_console_write(&buf[idx], 1, &write);

    // increment buffer
    ++idx;
  }

  //
  // Test code
  // Comment out the blocks to test output state
  //

  // ulog_info("TEST: Charging");
  // charge();
  // while (1) {}

  // ulog_info("TEST: Measuring");
  // measure();
  // while (1) {}

  return 0;
}
