#!/usr/bin/env bash
#
# collect_versions.sh
#
# Collects version information for common embedded-development tools
# (PlatformIO, OpenOCD, Python, Tockloader, elf2tab, ARM/RISC-V GCC
# toolchains, make) plus OS/kernel info, and prints a clean report.
#
# Usage:
#   ./collect_versions.sh            # print report to stdout
#   ./collect_versions.sh -o out.txt # also save report to a file
#
# Exit status is always 0; missing tools are reported as "not found"
# rather than causing the script to fail.

set -uo pipefail

OUTPUT_FILE=""

usage() {
    echo "Usage: $0 [-o output_file]"
    exit 1
}

while getopts ":o:h" opt; do
    case "$opt" in
        o) OUTPUT_FILE="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

# Collect everything into a variable first so it can be printed and,
# optionally, saved to a file in one pass.
REPORT=""

add_line() {
    REPORT+="$1"$'\n'
}

section() {
    add_line ""
    add_line "== $1 =="
}

# Run "$@" and return its first line of output, or "not found" /
# "error" if the command doesn't exist or fails. Some tools print
# their version to stderr instead of stdout, so we merge both streams.
get_version() {
    local cmd="$1"
    shift
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "not found"
        return
    fi
    local out
    out="$("$cmd" "$@" 2>&1)"
    if [ -z "$out" ]; then
        echo "error: no output from '$cmd $*'"
        return
    fi
    # Most --version output puts the useful info on the first line.
    echo "$out" | head -n 1
}

# ---------------------------------------------------------------------------
# OS / Kernel
# ---------------------------------------------------------------------------
section "Operating System / Kernel"

if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    add_line "OS:      ${PRETTY_NAME:-unknown}"
else
    add_line "OS:      not found (/etc/os-release missing)"
fi

add_line "Kernel:  $(uname -srvmo 2>/dev/null || echo 'not found')"

# ---------------------------------------------------------------------------
# Python
# ---------------------------------------------------------------------------
section "Python"

add_line "python3: $(get_version python3 --version)"
add_line "python:  $(get_version python --version)"

# ---------------------------------------------------------------------------
# PlatformIO
# ---------------------------------------------------------------------------
section "PlatformIO"

if command -v pio >/dev/null 2>&1; then
    add_line "pio:         $(get_version pio --version)"
elif command -v platformio >/dev/null 2>&1; then
    add_line "platformio:  $(get_version platformio --version)"
else
    add_line "platformio:  not found"
fi

# ---------------------------------------------------------------------------
# OpenOCD
# ---------------------------------------------------------------------------
section "OpenOCD"

add_line "openocd: $(get_version openocd --version)"

# ---------------------------------------------------------------------------
# Tockloader
# ---------------------------------------------------------------------------
section "Tockloader"

add_line "tockloader: $(get_version tockloader --version)"

# ---------------------------------------------------------------------------
# elf2tab
# ---------------------------------------------------------------------------
section "elf2tab"

add_line "elf2tab: $(get_version elf2tab --version)"

# ---------------------------------------------------------------------------
# ARM / RISC-V GCC toolchains
# ---------------------------------------------------------------------------
section "Cross-compiler toolchains"

add_line "arm-none-eabi-gcc:        $(get_version arm-none-eabi-gcc --version)"
add_line "riscv64-unknown-elf-gcc:  $(get_version riscv64-elf-gcc --version)"

# ---------------------------------------------------------------------------
# make
# ---------------------------------------------------------------------------
section "make"

add_line "make: $(get_version make --version)"


# ---------------------------------------------------------------------------
# protobuf
# ---------------------------------------------------------------------------
section "Protobuf"

add_line "protoc: $(get_version protoc --version)"
add_line "nanopb: $(get_version nanopb_generator --version)"


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
add_line ""

echo "$REPORT"

if [ -n "$OUTPUT_FILE" ]; then
    echo "$REPORT" > "$OUTPUT_FILE"
    echo "Report also saved to: $OUTPUT_FILE"
fi
