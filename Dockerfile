# ubuntu:18.04 base (down from 20.04 for the 16.0.0 port): the 16.0.0 prebuilt
# tarball ships as x86_64-linux-gnu-ubuntu-18.04 and links libtinfo.so.5 (the
# ncurses5/6 split), which bare ubuntu:20.04 does NOT provide (only
# libtinfo.so.6) — the 20.04 base cannot load the prebuilt opt/clang.
# ubuntu:18.04 (glibc 2.27, gcc 7.5) loads them cleanly (spike: `ldd opt`/
# `ldd clang` -> 0 "not found" symbols; max symbol requirement
# GLIBCXX_3.4.21 <= 18.04 libstdc++ GLIBCXX_3.4.25, so no link shim is
# needed, unlike the 15.0.0 rhel-8.4 prebuilt). The 16.0.0 prebuilt is built
# with the C++17 standard (per `llvm-config --cxxflags` — 16.0.0 is the first
# release built with C++17 by default; the new hard toolchain floor is GCC
# >= 7.1); 18.04's gcc 7.5 meets it.
#
# Toolchain provenance (carried from the 14.0.0/15.0.0 ports): LLVM stopped
# shipping prebuilt binaries on releases.llvm.org after 9.0.0; from 10.0.0
# onward the prebuilts live on the GitHub Releases of llvm/llvm-project (tag
# llvmorg-16.0.0). install_llvm.sh downloads the ubuntu-18.04 x86_64 tarball
# from there. If the tarball route ever breaks, the documented fallback is a
# 16.0.0 source build (monorepo tarball on the same GitHub release).
FROM ubuntu:18.04

ENV LLVM_HOME=/usr/local/llvm
ENV BuildMode=Release+Asserts
ENV TEST_PARALLELISM=seq
ENV PATH=/usr/local/llvm/bin:$PATH
# Non-interactive apt: keep dpkg from prompting (tzdata and friends ask a
# timezone question during an unattended `apt-get upgrade`). noninteractive
# makes dpkg take the defaults (UTC).
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev libxml2-dev

# Install CMake. The 16.0.0 prebuilt's LLVMConfigVersion.cmake sets no CMake
# version floor (it only gates on the LLVM package version, 16.0.0); the
# binding constraint is the project's own `cmake_minimum_required`. The 16.0.0
# source CMakeLists floor is 3.20 (soft in 16.x, hard from 17.x); the root
# floor here is 3.5, which is also the CMake 4.0 compatibility boundary (CMake
# 4.x drops `cmake_minimum_required(VERSION < 3.5)`). Pin the newest 3.31.x
# binary (3.31.12): 3.31.x keeps full compatibility with 3.5-era projects,
# satisfies the 16.0.0 3.20 requirement, and is the newest line verified to
# configure the 15.0.0 prebuilt.
RUN wget -q https://cmake.org/files/v3.31/cmake-3.31.12-linux-x86_64.tar.gz && \
    tar -xzf cmake-3.31.12-linux-x86_64.tar.gz && \
    cp -a cmake-3.31.12-linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.31.12-linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.31.12-linux-x86_64.tar.gz cmake-3.31.12-linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 16.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
