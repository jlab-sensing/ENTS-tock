#!/usr/bin/env bash

set -e
set -u
set -o pipefail

# get test input
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <test_input>"
    exit 1
fi

BASE_CMD="elf-size-analyze -t arm-none-eabi- -w 120 -HaF"
ELF_PATH="$(pwd)/$1"

TEMP_FILE=$(mktemp --suffix .html)

# print to console
$BASE_CMD $ELF_PATH

# html
$BASE_CMD -W $ELF_PATH > $TEMP_FILE
xdg-open $TEMP_FILE

# plotly
$BASE_CMD $ELF_PATH --plotly --plotly-type treemap
$BASE_CMD $ELF_PATH --plotly --plotly-type sunburst

echo "Generated size report at ${TEMP_FILE}"
