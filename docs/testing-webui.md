# Web UI Testing Guide

This is a step by step documentation to flash both boards, bring up the web UI, and verify that the new user-config save flow works.

---

## Part 1 — Flash the STM32

The STM32 runs on Tock OS. Use `tockloader` to install the apps.

**1. Erase existing apps and install fresh**

```bash
cd /ENTS-tock/embedded/stm32
make install
```

This erases the existing app state and installs the current STM32 apps.

Then open the serial console:

```bash
tockloader listen
```

You should see startup logs such as:

```text
Core    === App Initialized ===
Core    Current user configuration:
```

If this is the first flash, the configuration may be empty and the app may wait for a new config. That is expected.

---

## 2. Flash the ESP32

```bash
cd /ENTS-tock/embedded/esp32
pio run -e release -t uploadfs
pio run -e release -t upload
```

If the ESP32 is not entering boot mode correctly, hold BOOT while pressing RESET, then release BOOT before uploading.

Open the monitor with the correct port:

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

## 3. Open the web UI

1. Connect to the WiFi access point shown by the ESP32 serial output.
   - Password: `ilovedirt`
2. Open a browser and visit `http://192.168.4.1`
3. Confirm that the ENTS Configuration page loads.

---

## 4. Save a new configuration

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

## 5. Verify the buffer-clear feature

### Step A — Create some buffered data

Let the STM32 run long enough for measurements to accumulate in the FIFO buffer.

### Step B — Enable buffer clearing

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
