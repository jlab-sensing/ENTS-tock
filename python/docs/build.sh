#!/usr/bin/env bash

# Build the ents package documentation.
#
#   ./build.sh          generate into build/html
#   ./build.sh --open   generate, then open it
#
# Set CI=true to turn Sphinx warnings into errors.

set -e
set -u
set -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

if ! command -v sphinx-build &>/dev/null; then
  echo "Command sphinx-build not found."
  echo "Install the docs dependencies from the package root:"
  echo "    pip install -e .[docs]"
  exit 1
fi

ARGS=(-b html . build/html)
if [ "${CI-}" == "true" ]; then
  # -W turns warnings into errors, --keep-going reports all of them rather
  # than stopping at the first.
  ARGS=(-W --keep-going "${ARGS[@]}")
fi

sphinx-build "${ARGS[@]}"

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
