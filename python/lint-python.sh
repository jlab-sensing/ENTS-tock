#!/usr/bin/env bash
set -e

pip install ruff --quiet
ruff check python
ruff format --check python
