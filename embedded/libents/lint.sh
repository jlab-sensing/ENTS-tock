#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

bold=$(tput bold)
normal=$(tput sgr0)

# only check unstaged if not in CI
if [ "${CI-}" != "true" ]; then
    $SCRIPT_DIR/../libtock-c/tools/check_unstaged.sh || exit
    export TOCK_NO_CHECK_UNSTAGED=1
fi

echo "Linting libents"

echo ""
echo "${bold}Generating compile_commands.json${normal}"
rm -rf build/
bear -- make -j $NUM_JOBS

echo ""
echo "${bold}Running clang-tidy${normal}"
if [ "${CI-}" == "true" ]; then
    run-clang-tidy -j $NUM_JOBS -source-filter='^(?!.*\.pb\.(c|h)$).*$' -header-filter='^(?!.*\.pb\.(c|h)$).*$'
else
    run-clang-tidy -fix -format -j $NUM_JOBS -source-filter='^(?!.*\.pb\.(c|h)$).*$' -header-filter='^(?!.*\.pb\.(c|h)$).*$'
fi

echo "${bold}Done.${normal}"
