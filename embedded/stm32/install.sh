#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# NOTE: this must match the chip layout in the kernel
APP_ADDR="0x08010000"

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)

APP_DIR=$SCRIPT_DIR/apps

echo "${bold}Clearing apps${normal}"
tockloader erase-apps

echo ""
echo "${bold}Installing core${normal}"
pushd $APP_DIR/core
if [ "${DEV-}" == "true" ]; then
	make install
else
	make
	tockloader install -a $APP_ADDR
fi
popd

echo ""
echo "${bold}Installing sensors${normal}"
pushd $APP_DIR/sensors
if [ "${DEV-}" == "true" ]; then
	make install
else
	make
	tockloader install -a $APP_ADDR
fi
popd

echo ""
echo "${bold}Board info${normal}"
if [ "${DEV-}" == "true" ]; then
	tockloader info
else
	tockloader info -a $APP_ADDR
fi

echo ""
echo "${bold}Done.${normal}"
