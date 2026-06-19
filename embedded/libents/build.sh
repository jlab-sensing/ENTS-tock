#!/usr/bin/env bash

set -e

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

echo "Building libents"
make CFLAGS=-Werror -j $NUM_JOBS
