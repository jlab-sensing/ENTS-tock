#!/usr/bin/env bash

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)

start_dir="${1:-.}"

find "$start_dir" -depth -type d -name build -exec rm -rf {} +

echo "${bold}All clean.${normal}"
