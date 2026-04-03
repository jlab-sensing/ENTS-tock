#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

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
make install
popd

echo ""
echo "${bold}Installing sensors${normal}"
pushd $APP_DIR/sensors
make install
popd

echo ""
echo "${bold}Done.${normal}"
