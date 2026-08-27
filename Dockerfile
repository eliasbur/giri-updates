# ubuntu:20.04 base (up from 18.04 for the 15.0.0 port): the 15.0.0 prebuilt
# tarball ships only as x86_64-linux-gnu-rhel-8.4 (glibc 2.28), so the base
# image must provide glibc >= 2.28. 18.04 (glibc 2.27) is too old; 20.04
# (glibc 2.31) loads the rhel-8.4 binaries cleanly (spike: `ldd opt` -> 0
# "not found" symbols). The 15.0.0 prebuilt is built with the C++14 standard
# (per `llvm-config --cxxflags`); 20.04's gcc 9.4 covers that.
#
# Toolchain provenance (carried from the 14.0.0 port): LLVM stopped shipping
# prebuilt binaries on releases.llvm.org after 9.0.0; from 10.0.0 onward the
# prebuilts live on the GitHub Releases of llvm/llvm-project (tag
# llvmorg-15.0.0). install_llvm.sh downloads the rhel-8.4 x86_64 tarball from
# there. If the tarball route ever breaks, the documented fallback is a
# 15.0.0 source build (monorepo tarball on the same GitHub release).
FROM ubuntu:20.04

ENV LLVM_HOME=/usr/local/llvm
ENV BuildMode=Release+Asserts
ENV TEST_PARALLELISM=seq
ENV PATH=/usr/local/llvm/bin:$PATH

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev libxml2-dev

# Install CMake. The 15.0.0 prebuilt requires CMake >= 3.13.4 to configure
# (its LLVMConfig.cmake no longer carries a `cmake_minimum_required` of its
# own; the package is found via CONFIG, and the prebuilt CMake modules target
# the 3.13.4-era API). Pin the newest 3.31.x binary (3.31.12): 3.31.x is the
# last major line that keeps full compatibility with older projects, and it is
# the newest line verified to configure the 15.0.0 prebuilt. (CMake 4.x drops
# compatibility with `cmake_minimum_required(VERSION < 3.5)`, which is why the
# root CMakeLists floor is bumped 3.4.3 -> 3.5 so the project stays
# forward-compatible; 3.31.12 is pinned here as the newest known-good binary.)
RUN wget -q https://cmake.org/files/v3.31/cmake-3.31.12-linux-x86_64.tar.gz && \
    tar -xzf cmake-3.31.12-linux-x86_64.tar.gz && \
    cp -a cmake-3.31.12-linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.31.12-linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.31.12-linux-x86_64.tar.gz cmake-3.31.12-linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 15.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
