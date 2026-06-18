#!/usr/bin/env bash

NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || 4)

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

bold=$(tput bold)
normal=$(tput sgr0)


echo ""
echo "${bold}Formatting examples${normal}"

for mkfile in `find . -maxdepth 5 -name Makefile`; do
	dir=`dirname $mkfile`
	if [ $dir == "." ]; then continue; fi
	# Skip directories with leading _'s, useful for leaving test apps around
	if [[ $(basename $dir) == _* ]]; then continue; fi

	if [ "${CI-}" == "true" ]; then
		echo "::group::Linting for $dir"
	fi


	pushd $dir > /dev/null

  echo ""
  echo "${bold}Generating compile_commands.json${normal}"
  rm -rf build/
  bear -- make -j $NUM_JOBS

  echo ""
  echo "${bold}Running clang-tidy${normal}"
	if [ "${CI-}" == "true" ]; then
    run-clang-tidy -j $NUM_JOBS
	else
    run-clang-tidy -fix -j $NUM_JOBS
  fi

  # TODO Add option to automatically fix and re-run if there is an error


	popd > /dev/null

	if [ "${CI-}" == "true" ]; then
		echo "::endgroup::"
	fi
done

echo ""
echo "${bold}All done.${normal}"
