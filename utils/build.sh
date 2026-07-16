#!/bin/bash

# Configure + build Giri against LLVM 8.0.0 with CMake, then run the test suite.
# Intended to run inside the Docker image (see Dockerfile), where $LLVM_DIR
# points at the prebuilt LLVM's CMake package.

set -e

: ${LLVM_DIR:=$LLVM_HOME/lib/cmake/llvm}

cmake -S . -B build -DLLVM_DIR=$LLVM_DIR
cmake --build build -j$(nproc)

make -C test test
