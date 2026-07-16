# Giri build/test environment for the LLVM 8.0.0 port.
#
# Ubuntu 18.04 is the release LLVM's 8.0.0 prebuilt binaries target, so we pin
# it here. Giri is always built and tested inside this container, never on the
# host. Build with:  docker build -t giri-llvm8 .
FROM ubuntu:18.04

ENV LLVM_HOME=/usr/local/llvm
ENV LLVM_DIR=/usr/local/llvm/lib/cmake/llvm
ENV PATH=/usr/local/llvm/bin:$PATH
ENV TEST_PARALLELISM=seq

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        wget xz-utils ca-certificates \
        cmake make g++ \
    && rm -rf /var/lib/apt/lists/*

ADD . giri

# Install the prebuilt LLVM/Clang 8.0.0 toolchain into $LLVM_HOME.
RUN giri/utils/install_llvm.sh 8.0.0

# Configure + build Giri with CMake against LLVM 8.0.0, then run the test suite.
RUN cd giri && utils/build.sh
