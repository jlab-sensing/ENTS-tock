#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

bold=$(tput bold)
normal=$(tput sgr0)

$SCRIPT_DIR/../libtock-c/tools/check_unstaged.sh || exit
export TOCK_NO_CHECK_UNSTAGED=1

echo "Linting libents"

echo ""
echo "${bold}Generating compile_commands.json${normal}"
rm -rf build/
bear -- make -j $NUM_JOBS

echo ""
echo "${bold}Running clang-tidy${normal}"
if [ "${CI-}" == "true" ]; then
    run-clang-tidy -source-filter='^(?!.*(nanopb|microlog)).*' -j $NUM_JOBS
else
    run-clang-tidy -fix -format -source-filter='^(?!.*(nanopb|microlog)).*' -j $NUM_JOBS
fi

echo "${bold}Done.${normal}"
