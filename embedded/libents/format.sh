#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

bold=$(tput bold)
normal=$(tput sgr0)

# only check unstaged if not in CI
if [ "${CI-}" != "true" ]; then
    $SCRIPT_DIR/../libtock-c/tools/check_unstaged.sh || exit
    export TOCK_NO_CHECK_UNSTAGED=1
fi

SRCS=$(find ./src/libents \
    \( -name "microlog" -o -name "nanopb" -o -name "*pb*" \) -prune -o \
    \( -name "*.c" -o -name "*.cc" -o -name "*.h" \) -print)

echo "Sources"
echo "$SRCS"

if [ "${CI-}" == "true" ]; then
    echo $SRCS | xargs clang-format -n -Werror
else
    echo $SRCS | xargs clang-format -i
fi

echo "${bold}Done.${normal}"
