# ubuntu:18.04 base: the LLVM 8.0.0 release notes require GCC >= 5.1, and
# ubuntu:14.04's gcc 4.8 is below that (18.04 ships gcc 7.5). Note: this run
# started as 16.04 but ubuntu:16.04 (xenial) is no longer served by
# old-releases.ubuntu.com, so we escalated to 18.04 per the plan's fallback —
# bionic is still on the normal archive.ubuntu.com (verified 200) and needs no
# repo redirection.
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

RUN giri/utils/install_llvm.sh 8.0.0

# Build step is done interactively via `source /giri/utils/build.sh`
