#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)


# get test input
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <test_input>"
    exit 1
fi

# get path to this script
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# get path to desired test
test_input="$script_dir/$1"

pushd $test_input > /dev/null

echo "${bold}Clearing apps${normal}"
tockloader erase-apps

echo ""
echo "${bold}Testing: $test_input${normal}"

make install
tockloader listen

popd > /dev/null


echo ""
echo "${bold}Done.${normal}"
