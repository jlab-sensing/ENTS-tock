#!/usr/bin/env bash

set -e

if ! command -v pio &>/dev/null; then
    echo "Error: 'pio' command not found. Install PlatformIO first."
    exit 1
fi

pio check
