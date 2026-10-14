#!/usr/bin/env bash

set -e
set -u
set -o pipefail

bold=$(tput bold)
normal=$(tput sgr0)

start_dir="${1:-.}"

# Define directories to exclude (add or remove directories as needed)
exclude_dirs=(
    "extras/tock"
)

# Build the exclusion arguments as an array — no string-based quoting issues
find_args=()
for dir in "${exclude_dirs[@]}"; do
    find_args+=(-not -path "*${dir}*")
done

find "$start_dir" -depth -type d -name build "${find_args[@]}" -print -exec rm -rf {} +

echo "${bold}All clean.${normal}"
