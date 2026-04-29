#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)

echo "${bold}Clearing apps${normal}"
tockloader erase-apps

echo ""
echo "${bold}Installing core${normal}"
pushd core
make install
popd

echo ""
echo "${bold}Installing sensors${normal}"
pushd apps/sensors
make install
popd

echo ""
echo "${bold}Done.${normal}"
