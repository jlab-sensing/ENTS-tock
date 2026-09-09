# ENTS-tock Getting Started

This document describes how to begin using the ENTS-tock application on an ENTS board.

## Windows (using Windows Subsystem for Linux WSL)
Note: All CLI commands should be executed within your WSL (unless otherwise specified).

1. Install WSL Ubuntu 26.04: https://learn.microsoft.com/en-us/windows/wsl/install
   - `wsl --install -d Ubuntu-26.04`
2. Install USBIPD: https://learn.microsoft.com/en-us/windows/wsl/connect-usb
   - Allow your WSL user to access the USB devices without needing sudo: Create file `/etc/udev/rules.d/usb.rules` with contents `SUBSYSTEM=="tty",MODE="0666",GROUP="dialout"`
   - `sudo udevadm control --reload-rules`
   - `sudo udevadm trigger`
3. Install Rust: https://rustup.rs/
   - `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`
   - Restart your WSL terminal to get the correct paths loaded.
4. Install other tools:
   - `sudo apt-get install openocd pipx unzip usbutils gcc-arm-none-eabi gcc-riscv64-unknown-elf`
   - Note: `openocd` >= 0.12 for STLINK-V3MINIE
   - `usbutils` is optional, but useful for `lsusb` to verify that USBIPD correctly passes through USB devices.
5. Install tockloader:
   - ~~`pipx install tockloader`~~
     - 2026-09-09 Workaround for `stlink_usb_error_check(): STLINK_SWD_AP_FAULT`:
       - Uninstall tockloader if already installed: `pipx uninstall tockloader`
       - Install the workaround: `pipx install --force "git+https://github.com/tyler-potyondy/tockloader.git@ents-patch"`
   - `pipx ensurepath`
6. Install PlatformIO:
   - `curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py`
   - `python3 get-platformio.py`
   - Install shell commands for PlatformIO
     - In `~/.bashrc` add this to the end of the file if it isn't there yet (but this is usually already added by `pipx ensurepath`): `export PATH=$PATH:$HOME/.local/bin`
   - Add symlinks for the executables:
     - `ln -s ~/.platformio/penv/bin/platformio ~/.local/bin/platformio`
     - `ln -s ~/.platformio/penv/bin/pio ~/.local/bin/pio`
     - `ln -s ~/.platformio/penv/bin/piodebuggdb ~/.local/bin/piodebuggdb`
   - PlatformIO udev rules: https://docs.platformio.org/en/latest/core/installation/udev-rules.html
     - `curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules`
     - `sudo udevadm control --reload-rules`
     - `sudo udevadm trigger`
7. Clone the ENTS-tock repo:
   - `git clone git@github.com:jlab-sensing/ENTS-tock.git`
   - `cd ENTS-tock`
   - `git submodule update --init --recursive`
9. (Possibly optional, python=python3) Create a Python virtual environment:
   - `cd embedded`
   - `python3 -m venv .venv`
   - `source ./.venv/bin/activate`
10. Bind and attach the USB devices:
   - (in Windows cmd / PS) `usbipd list`
   - (in Windows cmd / PS) `usbipd bind -b <bus_id>`
   - (in Windows cmd / PS) `usbipd attach --wsl -b <bus_id>`
     - If USBIPD fails to attach a device to WSL: (in WSL) `sudo modprobe vhci_hcd`
11. Build and install ENTS-tock onto the ENTS board:
   - `./install.sh`
   - If the ESP32 fails to flash:
     - Ensure that the ESP32 is in the bootloader mode when it is being flashed. To enter the ESP32 bootloader mode: While holding down the BOOT button (SW4), press and release the RESET button (SW3). (The BOOT button can be released after releasing the RESET button.)
       - [ENTS Hardware v3.1.0] On v3.1.0 hardware, the ESP32 enable EN pin is controlled by the STM32, so the ESP32 must be enabled by the STM32 in order to receive new programs. This is typically only needed if the ENTS board has not been previously programmed, since most programs enable the ESP32 during initialization. This can be done by flashing [ENTS-node-firmware](https://github.com/jlab-sensing/ENTS-node-firmware/) `pio run -e example_controller_wifi -t upload` or `pio run -e stm32 -t upload`.
         - TODO: Flash a small ENTS-tock program to the STM32 first in order to enable the ESP32 and LED blink?
     - Ensure that no other program currently has the USB to TTL adapter open in a serial monitor.
     - If the USB to TTL adapter is not enumerated as `/dev/ttyUSB0`, then you must modify `esp32/platformio.ini` such that the `upload_port`, `monitor_port`, and `test_port` match the attached USB to TTL adapter. If you have multiple `/dev/ttyUSB*` devices, you can distinguish between them using `pio device list` or detaching & reattaching them via (Windows) USBIPD `usbipd detach -b <bus_id>` then `usbipd attach --wsl -b <bus_id>`.

## MacOS
- TODO

## Linux
- TODO

# References
- https://github.com/tock/tock/blob/master/doc/Getting_Started.md
- https://book.tockos.org/setup/quickstart-windows
- https://book.tockos.org/setup/quickstart-linux
