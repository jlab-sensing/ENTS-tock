# Getting started

A short tour of the ENTS board: what it is, how firmware gets onto it, how to
configure it, and how to see its data.

## How the board fits together

An ENTS node has two microcontrollers:

- **STM32** — runs [Tock OS](https://www.tockos.org/) and the sensing
  applications. It reads the sensors and decides when to upload.
- **ESP32-C3** — a coprocessor that handles WiFi: it hosts the configuration
  portal, uploads measurements to the configured API endpoint, and writes logs
  to the microSD card.

Data flow: sensors → STM32 → I2C → ESP32 → WiFi →
[DirtViz](https://dirtviz.jlab.ucsc.edu/).

## Build and flash

With the toolchain installed, clone the repository recursively and run the
install script from `embedded/` with both programmers attached:

```bash
git clone git@github.com:jlab-sensing/ENTS-tock.git
cd ENTS-tock
git submodule update --init --recursive
cd embedded
./install.sh
```

This flashes the ESP32 firmware, installs the Tock kernel on the STM32, and
installs the STM32 applications, in that order.

If the ESP32 does not accept the upload, put it in bootloader mode: hold
`BOOT` (SW4), press and release `RESET` (SW3), then release `BOOT`.

## Configure the board

The ESP32 hosts a WiFi configuration portal — no internet required.

1. Power the board and connect to the access point it starts
   (`ESP32-C3-Config`).
2. Open `http://192.168.4.1` in a browser.
3. Fill in the configuration — logger and cell IDs, upload interval, enabled
   sensors, calibration coefficients, WiFi credentials, and the API endpoint —
   and save.

The same configuration can be written over serial with the `ents` CLI from the
python package.

## Check that data flows

Give the node one upload interval, then open
[DirtViz](https://dirtviz.jlab.ucsc.edu/) and find the cell you configured —
data is keyed by Cell ID. The ESP32 serial console logs each upload attempt
and its response, which makes it the first place to look when nothing
arrives.

## Where to go next

- [Embedded API reference](https://jlab-sensing.github.io/ENTS-tock/embedded/index.html)
- [ents python package](https://jlab-sensing.github.io/ENTS-tock/python/index.html)
- [Tock OS book](https://book.tockos.org/)
