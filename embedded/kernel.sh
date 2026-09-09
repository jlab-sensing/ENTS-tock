#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Installing kernel"
pushd $SCRIPT_DIR/extras/tock/boards/lora_e5_mini > /dev/null

# reset chip layout
git restore chip_layout.ld


if [ "${DEV-}" == "true" ]; then
    echo "Development build"

    make erase-all
    make clean    
    make install dev=1
else
    echo "Production build (no console)"

    # Apply chip layout patch for smaller sized kernel
    git apply prod-layout.patch

    make erase-all
    make clean    
    make install
fi

popd > /dev/null
echo ""
