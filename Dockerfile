# ubuntu:18.04 base (unchanged from the 8.0.0 image): the 14.0.0 prebuilt
# tarball is built for ubuntu-18.04, and 18.04's gcc 7.5 covers the C++14
# standard that LLVM 14 builds with (per `llvm-config --cxxflags`).
#
# Toolchain provenance (new in the 14.0.0 port): LLVM stopped shipping
# prebuilt binaries on releases.llvm.org after 9.0.0; from 10.0.0 onward the
# prebuilts live on the GitHub Releases of llvm/llvm-project (tag
# llvmorg-14.0.0). install_llvm.sh downloads the ubuntu-18.04 x86_64 tarball
# from there. If the tarball route ever breaks, the documented fallback is a
# 14.0.0 source build (monorepo tarball on the same GitHub release).
FROM ubuntu:18.04

ENV LLVM_HOME=/usr/local/llvm
ENV BuildMode=Release+Asserts
ENV TEST_PARALLELISM=seq
ENV PATH=/usr/local/llvm/bin:$PATH

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev libxml2-dev

# Install CMake >= 3.4.3 (Ubuntu 18.04 ships 3.10; pin a known-good 3.12 binary)
RUN wget -q https://cmake.org/files/v3.12/cmake-3.12.4-Linux-x86_64.tar.gz && \
    tar -xzf cmake-3.12.4-Linux-x86_64.tar.gz && \
    cp -a cmake-3.12.4-Linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.12.4-Linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.12.4-Linux-x86_64.tar.gz cmake-3.12.4-Linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 14.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
