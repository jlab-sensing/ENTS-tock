#!/usr/bin/env bash

set -e

if command -v ruff &>/dev/null; then
  echo "Command ruff not found."
  echo "Install development dependencies with 'pip install -e .[dev]'."
fi

if [ "${CI-}" == "true" ]; then
    ruff format --check
else
    ruff format
fi
