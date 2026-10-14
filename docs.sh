#!/usr/bin/env bash

# Generates html documentation for project.
#
# Generates code from doxygen for embedded c soure code and sphinx for python.
# Adds a nice HTML frontpage for directing to relavent documentation.

set -e
set -u
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# link to docs folder
DOCS_DIR="${SCRIPT_DIR}/docs"

# link to build dir to store html files
BUILD_DIR="${DOCS_DIR}/build"

# generated docs paths to copy from
EMBEDDED_DIR="${SCRIPT_DIR}/embedded/doxygen/build/html"
PYTHON_DIR="${SCRIPT_DIR}/python/docs/build/html"


if [ "${CI-}" == "true" ]; then
	echo "::group::Building embedded docs"
fi

echo "Build embedded docs"
pushd $SCRIPT_DIR/embedded/doxygen > /dev/null
./build.sh
popd > /dev/null
echo ""

if [ "${CI-}" == "true" ]; then
	echo "::endgroup::"
fi


if [ "${CI-}" == "true" ]; then
	echo "::group::Building python docs"
fi

echo "Build python docs"
pushd $SCRIPT_DIR/python/docs > /dev/null
./build.sh
popd > /dev/null
echo ""

if [ "${CI-}" == "true" ]; then
	echo "::endgroup::"
fi


if [ "${CI-}" == "true" ]; then
	echo "::group::Combining static files"
fi

echo "Folder setup"
pushd $DOCS_DIR > /dev/null

rm -rf build/

# create build directory
mkdir -p build
cp index.html build/
# Pages runs the output through Jekyll otherwise, which drops any
# file or directory beginning with an underscore. Sphinx emits
# _static and _sources.
touch build/.nojekyll

# copy embedded
mkdir -p build/embedded
cp -r $EMBEDDED_DIR/. build/embedded

# copy python
mkdir -p build/python
cp -r $PYTHON_DIR/. build/python

popd > /dev/null

if [ "${CI-}" == "true" ]; then
	echo "::endgroup::"
fi

echo "Index page for docs: ${BUILD_DIR}/index.html"
