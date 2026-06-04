#!/usr/bin/env bash
set -e

pip install platformio --quiet
cd embedded/esp32
pio check -e release --fail-on-defect high
