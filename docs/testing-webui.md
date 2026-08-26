# Web UI Testing Guide

This is a step by step documentation to flash both boards, bring up the web UI, and verify that the new user-config save flow works.

---

## Part 1 - Installing firmware

Use the following bash script to install stm32 kernel, stm32 core app, stm32 sensors app, esp32 firmware, and esp32 html files.

```
./embedded/install.sh
```

### STM32

You can view the serial printout with the following.

```bash
tockloader listen
```

You should see something similar to these startup logs.

```text
Core    === App Initialized ===
Core    Current user configuration:
```

If this is the first flash, the configuration may be empty and the app may wait for a new config. That is expected.


### ESP32


Open the monitor with the correct port.

```bash
pio device monitor
```

You should see logs similar to:

```text
WiFi AP Info:
ssid "ents-XXXXXXXX"
pass "ilovedirt"
User Config http://192.168.4.1/
```

---

## 2. Open the web UI

1. Connect to the WiFi access point shown by the ESP32 serial output.
   - Password: `ilovedirt`
2. Open a browser and visit `http://192.168.4.1`
3. Confirm that the ENTS Configuration page loads.

---

## 3. Save a new configuration

1. Fill in a valid config.
   - Logger ID: `1`
   - Cell ID: `1`
   - Upload Method: `WiFi`
   - Upload Interval: `30`
   - Select at least one sensor
   - Fill the calibration fields with simple values such as `1`, `0`, `1`, `0`
2. Enter a WiFi SSID and password if needed.
3. Leave the buffer-clear checkbox unchecked for a normal save.
4. Click Save Configuration.

Expected result:
- The success modal appears.
- The ESP32 serial log shows the new config values.
- The STM32 serial log shows the updated configuration.

---

## 4. Verify the buffer-clear feature

### Step A - Create some buffered data

Let the STM32 run long enough for measurements to accumulate in the FIFO buffer.

### Step B - Enable buffer clearing

1. Re-open the web UI.
2. Check the red **Clear buffer on save** box.
3. Click Save Configuration.

Expected result:
- The ESP32 log shows: `clear_buffer_on_save requested — STM32 will clear FRAM on next boot`
- The STM32 log shows:

```text
clear_buffer flag set — clearing FRAM measurement buffer...
FIFO buffer length before clear: N
fifo_buffer_clear: OK
FIFO buffer length after clear: 0
```

That confirms the buffer was cleared.

---

## 6. Quick sanity checks

- Refresh the browser page and confirm the saved values reappear.
- Try saving with an empty logger ID or empty WiFi SSID to confirm validation errors appear.
- If a config is not updating, reset the STM32 and check the serial logs again.
