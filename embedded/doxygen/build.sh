#!/usr/bin/env bash

# Generate the embedded source documentation.
#
#   ./build.sh          generate into build/html
#   ./build.sh --open   generate, then open it in a browser

set -e
set -u
set -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

if ! command -v doxygen &>/dev/null; then
  echo "Command doxygen not found."
  echo "  macOS:  brew install doxygen"
  echo "  Ubuntu: sudo apt install doxygen"
  echo "  Windows: choco install doxygen.install, or scoop install doxygen"
  exit 1
fi

mkdir -p build
doxygen Doxyfile

echo
echo "Docs written to $HERE/build/html/index.html"

if [ "${1-}" == "--open" ]; then
  INDEX="build/html/index.html"
  if command -v xdg-open &>/dev/null; then xdg-open "$INDEX"
  elif command -v open &>/dev/null; then open "$INDEX"
  elif command -v start &>/dev/null; then start "$INDEX"
  else echo "Open it yourself: $HERE/$INDEX"
  fi
fi
