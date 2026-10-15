#!/usr/bin/env bash
# Build and run the uptime host unit tests.
#
# Compiles the real libents/util/uptime.c against the stub drivers in stubs/,
# so no board and no toolchain beyond a host compiler are needed.
set -e
set -u
set -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../../src/libents"
CC="${CC:-gcc}"

mkdir -p "$HERE/build"

"$CC" -std=c11 -Wall -Wextra -Werror -g \
  -I "$HERE/stubs" \
  "$HERE/main.c" "$SRC/util/uptime.c" \
  -o "$HERE/build/uptime_test"

"$HERE/build/uptime_test"
