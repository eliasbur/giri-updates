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
# Non-interactive apt: on 20.04 the 2025 security updates to tzdata (and
# friends) ask a timezone question during `apt-get upgrade`, which hangs an
# unattended build. noninteractive makes dpkg take the defaults (UTC).
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev libxml2-dev

# Install CMake. The 15.0.0 prebuilt's LLVMConfigVersion.cmake sets no CMake
# version floor (it only gates on the LLVM package version); the binding
# constraint is the project's own `cmake_minimum_required`. The 15.0.0 source
# CMakeLists floor is 3.5, so the root floor is bumped 3.4.3 -> 3.5 — which is
# also the CMake 4.0 compatibility boundary (CMake 4.x drops
# `cmake_minimum_required(VERSION < 3.5)`). Pin the newest 3.31.x binary
# (3.31.12): 3.31.x keeps full compatibility with 3.5-era projects and is the
# newest line verified to configure the 15.0.0 prebuilt.
RUN wget -q https://cmake.org/files/v3.31/cmake-3.31.12-linux-x86_64.tar.gz && \
    tar -xzf cmake-3.31.12-linux-x86_64.tar.gz && \
    cp -a cmake-3.31.12-linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.31.12-linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.31.12-linux-x86_64.tar.gz cmake-3.31.12-linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 15.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
