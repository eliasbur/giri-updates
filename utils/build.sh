#!/bin/bash

# Configure + build Giri against LLVM 8.0.0 with CMake, then run the test suite.
# Intended to run inside the Docker image (see Dockerfile), where $LLVM_DIR
# points at the prebuilt LLVM's CMake package.

set -e

: ${LLVM_DIR:=$LLVM_HOME/lib/cmake/llvm}

mkdir -p build
cd build
cmake -DLLVM_DIR=$LLVM_DIR ..
cmake --build . -- -j$(nproc)
cd ..

make -C test test
