# ubuntu:20.04 base (deviation from the 18.04 convention used by the 8/14/15/16
# images, forced by asset availability): the llvmorg-12.0.0 GitHub release ships
# no x86_64 ubuntu-18.04 prebuilt (x86_64 assets are ubuntu-16.04,
# ubuntu-20.04, and sles12.4), so the 18.04 base has no matching tarball.
# 20.04 ships gcc 9.3 (far above LLVM 12's GCC >= 5.1 requirement) and matches
# the chosen tarball distro, keeping the toolchain on a single consistent base.
# Fallback (last resort, not used): ubuntu:16.04 + its 12.0.0 tarball, which
# requires old-releases.ubuntu.com repo redirection.
FROM ubuntu:20.04

ENV LLVM_HOME=/usr/local/llvm
ENV BuildMode=Release+Asserts
ENV TEST_PARALLELISM=seq
ENV PATH=/usr/local/llvm/bin:$PATH
# Keep apt non-interactive: on ubuntu:20.04, `apt-get upgrade` pulls tzdata and
# otherwise blocks on its geographic-area prompt (TERM unset in buildkit).
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev libxml2-dev

# Install CMake >= 3.4.3 (Ubuntu 20.04 ships 3.16; pin the same known-good 3.12 binary as 8/14)
RUN wget -q https://cmake.org/files/v3.12/cmake-3.12.4-Linux-x86_64.tar.gz && \
    tar -xzf cmake-3.12.4-Linux-x86_64.tar.gz && \
    cp -a cmake-3.12.4-Linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.12.4-Linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.12.4-Linux-x86_64.tar.gz cmake-3.12.4-Linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 12.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
