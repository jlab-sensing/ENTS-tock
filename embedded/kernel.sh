#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Installing kernel"
pushd $SCRIPT_DIR/extras/tock/boards/lora_e5_mini > /dev/null
if [ "${DEV-}" == "true" ]; then
    echo "Development build"
    echo "NOT IMPLEMENTED"
else
    echo "Production build"
    make install
fi
popd > /dev/null
echo ""
