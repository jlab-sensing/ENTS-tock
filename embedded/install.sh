#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# TODO
echo ""
echo "TODO: Add script that clones, builds, and installs the kernel."
echo ""

echo "Installing firmware on esp32"
pushd $SCRIPT_DIR/esp32 > /dev/null
./install.sh
popd > /dev/null
echo ""

echo "Installing firmware on stm32"
pushd $SCRIPT_DIR/stm32 > /dev/null
./install.sh
popd > /dev/null
echo ""
