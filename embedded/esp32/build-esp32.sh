#!/usr/bin/env bash
set -e

pip install platformio --quiet
cd embedded/esp32
pio run -e release
