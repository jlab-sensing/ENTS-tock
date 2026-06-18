#!/usr/bin/env bash

set -e

if ! command -v ruff &>/dev/null; then
  echo "Command ruff not found."
  echo "Install development dependencies with 'pip install -e .[dev]'."
  exit 1
fi

if [ "${CI-}" == "true" ]; then
    ruff check
else
    ruff check --fix
fi
