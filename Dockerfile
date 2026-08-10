FROM ubuntu:14.04

ENV LLVM_HOME=/usr/local/llvm
ENV BuildMode=Release+Asserts
ENV TEST_PARALLELISM=seq
ENV PATH=/usr/local/llvm/bin:$PATH

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -qq -y wget make g++ python zip unzip autoconf libtool automake xz-utils libtinfo-dev zlib1g-dev libncurses5-dev libedit-dev libz-dev

# Install CMake >= 3.4.3 (Ubuntu 14.04 ships 2.8)
RUN wget -q https://cmake.org/files/v3.12/cmake-3.12.4-Linux-x86_64.tar.gz && \
    tar -xzf cmake-3.12.4-Linux-x86_64.tar.gz && \
    cp -a cmake-3.12.4-Linux-x86_64/bin/* /usr/local/bin/ && \
    cp -a cmake-3.12.4-Linux-x86_64/share/* /usr/local/share/ && \
    rm -rf cmake-3.12.4-Linux-x86_64.tar.gz cmake-3.12.4-Linux-x86_64

ADD . giri

RUN giri/utils/install_llvm.sh 5.0.2

# Build step is done interactively via `source /giri/utils/build.sh`
