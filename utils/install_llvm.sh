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
	"14.0.0")
		# Use prebuilt release tarball — no build from source needed.
		# NOTE: from LLVM 10.0.0 onward, prebuilt binaries no longer live on
		# releases.llvm.org (that site keeps only sources/docs from 10.0.0
		# on) — they moved to the GitHub Releases of llvm/llvm-project (tag
		# llvmorg-14.0.0). The ubuntu-18.04 x86_64 tarball matches the
		# Dockerfile base (giri-llvm-14 is ubuntu:18.04; gcc 7.5 covers the
		# C++14 standard 14.0.0 builds with). Fallback if the tarball route
		# ever breaks: build 14.0.0 from source (llvm-project monorepo
		# tarball, also on the same GitHub release).
		wget -q https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		tar xf clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		rm -f clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		mv clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04 $LLVM_HOME
		;;
		"15.0.0")
			# Use prebuilt release tarball — no build from source needed.
			# NOTE: from LLVM 10.0.0 onward, prebuilt binaries live on the
			# GitHub Releases of llvm/llvm-project (tag llvmorg-15.0.0). 15.0.0
			# ships exactly ONE x86_64-linux prebuilt: the rhel-8.4 tarball
			# (verified against the GitHub API) — there is no ubuntu-18.04 or
			# ubuntu-20.04 asset. rhel-8.4 is glibc 2.28, so the Dockerfile
			# base is ubuntu:20.04 (glibc 2.31); the spike confirmed the
			# binaries load with 0 unsatisfied glibc symbols. The 15.0.0
			# prebuilt is C++14 (per `llvm-config --cxxflags`); 20.04's gcc 9.4
			# covers that. Fallback if the tarball route ever breaks: build
			# 15.0.0 from source (llvm-project monorepo tarball, also on the
			# same GitHub release).
			wget -q https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.0/clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz
			tar xf clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz
			rm -f clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz
			mv clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4 $LLVM_HOME
			;;
		"16.0.0")
			# Use prebuilt release tarball — no build from source needed.
			# NOTE: from LLVM 10.0.0 onward, prebuilt binaries live on the
			# GitHub Releases of llvm/llvm-project (tag llvmorg-16.0.0). 16.0.0
			# ships the x86_64-linux-gnu ubuntu-18.04 asset (verified against
			# the GitHub API: clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.
			# tar.xz, 966,785,280 bytes) plus other non-x86_64-gnu builds. The
			# prebuilt is an ubuntu-18.04 build linking libtinfo.so.5, so the
			# Dockerfile base is ubuntu:18.04 (glibc 2.27, gcc 7.5): the spike
			# showed ldd clean on opt/clang (0 missing) and the max symbol
			# requirement GLIBCXX_3.4.21 <= 18.04's libstdc++ GLIBCXX_3.4.25,
			# so no link shim is needed (unlike the 15.0.0 rhel-8.4 prebuilt).
			# 16.0.0 is built with C++17 (per `llvm-config --cxxflags`); gcc 7.5
			# meets the new hard GCC >= 7.1 requirement. Fallback if the
			# tarball route ever breaks: build 16.0.0 from source (llvm-project
			# monorepo tarball, also on the same GitHub release).
			wget -q https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.0/clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
			tar xf clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
			rm -f clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
			mv clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04 $LLVM_HOME
			;;
esac
