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

# Explicitly build no targets if we're only formatting.
#
# This allows a system (i.e., a CI instance) with no cross-compilers to still
# format without generating any warnings.
export TOCK_TARGETS=

echo ""
echo "${bold}Formatting examples${normal}"

for mkfile in `find . -maxdepth 5 -name Makefile`; do
	dir=`dirname $mkfile`
	if [ $dir == "." ]; then continue; fi
	# Skip directories with leading _'s, useful for leaving test apps around
	if [[ $(basename $dir) == _* ]]; then continue; fi

	if [ "${CI-}" == "true" ]; then
		echo "::group::Format for $dir"
	fi

	pushd $dir > /dev/null
	echo ""
	echo "Formatting $dir"

  SRCS=`find . \( -name "*.c" -o -name "*.cc" -o -name "*.h" \)`
	if [ "${CI-}" == "true" ]; then
    echo $SRCS | xargs clang-format -n -Werror
  else
    echo $SRCS | xargs clang-format -i
	fi

	popd > /dev/null

	if [ "${CI-}" == "true" ]; then
		echo "::endgroup::"
	fi
done

echo ""
echo "${bold}All formatted.${normal}"
