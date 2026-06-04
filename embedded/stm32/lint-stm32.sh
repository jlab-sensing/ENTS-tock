#!/usr/bin/env bash
set -e

cppcheck --error-exitcode=1 \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    embedded/stm32/core/ \
    embedded/libents/src/libents/
