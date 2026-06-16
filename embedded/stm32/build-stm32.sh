#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)

for mkfile in $(find embedded/stm32 -maxdepth 6 -name Makefile); do
  dir=`dirname $mkfile`
  if [ $dir == "." ]; then continue; fi
  # Skip directories with leading _'s, useful for leaving test apps around
  if [[ $(basename $dir) == _* ]]; then continue; fi

  # clear existing apps
  echo "${bold}Clearing apps${normal}"

  pushd $dir > /dev/null
  echo ""
  echo "${bold}Testing: $dir${normal}"
  echo "Close serial monitor to continue..."
  make

  popd > /dev/null
done
