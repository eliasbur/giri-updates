#!/bin/bash

# USAGE: export LLVM_HOME=llvm && $0 [VERSION]

if [ "giri$LLVM_HOME" == "giri" ]; then
	echo "Please set the LLVM_HOME env!"
	exit 1
fi

VERSION=3.4
[ $# -ge 1 ] && VERSION=$1

case $VERSION in
	"3.1")
		;;
	"3.4")
		wget https://releases.llvm.org/3.4/clang-3.4.src.tar.gz
		wget https://releases.llvm.org/3.4/llvm-3.4.src.tar.gz
		wget https://releases.llvm.org/3.4/compiler-rt-3.4.src.tar.gz
		tar xf llvm-3.4.src.tar.gz && rm -f llvm-3.4.src.tar.gz
		tar xf clang-3.4.src.tar.gz && rm -f clang-3.4.src.tar.gz
		tar xf compiler-rt-3.4.src.tar.gz && rm -f compiler-rt-3.4.src.tar.gz
		mv llvm-3.4 $LLVM_HOME
		mv clang-3.4 $LLVM_HOME/tools/clang
		mv compiler-rt-3.4 $LLVM_HOME/projects/compiler-rt

		# Build from source (autoconf)
		mkdir -p $LLVM_HOME/build
		cd $LLVM_HOME/build
		../configure --enable-optimized \
					 --disable-debug-symbols \
					 --disable-docs \
					 --disable-terminfo \
					 --disable-bindings \
					 --enable-targets=host-only \
					 --enable-shared
		make -j$(nproc)
		make install
		;;
	"5.0.2")
		# Use prebuilt release tarball — no build from source needed
		wget https://releases.llvm.org/5.0.2/clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-14.04.tar.xz
		tar xf clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-14.04.tar.xz
		rm -f clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-14.04.tar.xz
		mv clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-14.04 $LLVM_HOME
		;;
	"8.0.0")
		# Use prebuilt release tarball — no build from source needed.
		# Requires the ubuntu:18.04 base image (see Dockerfile): the LLVM 8
		# release notes require GCC >= 5.1, and ubuntu:14.04's gcc 4.8 is below
		# that. (LLVM 8's own build standard is C++11 per `llvm-config
		# --cxxflags`; the 14.04 gcc predates the required 5.1.) (The 16.04
		# tarball would work too, but xenial apt repos are dead, so the
		# Dockerfile uses 18.04.)
		wget https://releases.llvm.org/8.0.0/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		tar xf clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		rm -f clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		mv clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04 $LLVM_HOME
		;;
	"12.0.0")
		# Use prebuilt release tarball — no build from source needed.
		# From LLVM 10.0.0 onward the prebuilts no longer live on
		# releases.llvm.org (sources/docs only) — they moved to the GitHub
		# Releases of llvm/llvm-project (tag llvmorg-12.0.0). This release
		# ships no x86_64 ubuntu-18.04 asset (x86_64 assets are ubuntu-16.04,
		# ubuntu-20.04, sles12.4), so the image uses the ubuntu-20.04 asset
		# on an ubuntu:20.04 base (see Dockerfile) — a documented deviation
		# from the 18.04 convention of the 8/14 images.
		wget https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz
		tar xf clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz
		rm -f clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz
		mv clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04 $LLVM_HOME
		;;
esac
