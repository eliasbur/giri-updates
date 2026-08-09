#!/bin/bash

# Build Giri using CMake and run tests.
# Safe to source repeatedly.
# Expects to be run/sourced from the Giri source root, or with GIRI_ROOT set.

GIRI_ROOT="${GIRI_ROOT:-$(pwd)}"

mkdir -p "${GIRI_ROOT}/build"
cd "${GIRI_ROOT}/build"

# Configure if not already done, or if CMakeCache.txt is older than CMakeLists.txt
if [ ! -f CMakeCache.txt ] || [ "${GIRI_ROOT}/CMakeLists.txt" -nt CMakeCache.txt ]; then
  cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        "${GIRI_ROOT}"
fi

make -j"$(nproc)"
cd "${GIRI_ROOT}"
make -C test