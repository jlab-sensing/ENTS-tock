#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Install production version of kernel
$SCRIPT_DIR/kernel.sh

echo "Installing firmware on stm32"
pushd $SCRIPT_DIR/stm32 > /dev/null
./install.sh
popd > /dev/null
echo ""

echo "Installing firmware on esp32"
pushd $SCRIPT_DIR/esp32 > /dev/null
./install.sh
popd > /dev/null
echo ""
