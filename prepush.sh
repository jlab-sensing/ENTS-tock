#!/usr/bin/env bash

set -e

export CI=true

echo "Clean"
pushd ./embedded > /dev/null
./clean.sh
popd > /dev/null

echo "Prepush libents"
pushd ./embedded/libents > /dev/null
./lint.sh
./format.sh
./build.sh
popd > /dev/null

echo "Prepush stm32"
pushd ./embedded/stm32 > /dev/null
./lint.sh
./format.sh
./build_all.sh
./test_all.sh
popd > /dev/null

echo "Prepush esp32"
pushd ./embedded/esp32 > /dev/null
./lint.sh
./format.sh
./build_all.sh
./test_all.sh
popd > /dev/null

echo "Prepush proto"
pushd ./proto > /dev/null
echo "Not implemented check for changes"
popd > /dev/null

echo "Prepush docs"
pushd ./proto > /dev/null
echo "Not implemented yet"
popd > /dev/null

echo "Prepush python"
pushd ./python > /dev/null
./lint.sh
./format.sh
./build.sh
./test.sh
popd > /dev/null
