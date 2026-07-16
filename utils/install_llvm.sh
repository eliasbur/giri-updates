#!/bin/bash

# USAGE: export LLVM_HOME=/usr/local/llvm && $0 [VERSION]
#
# Installs a prebuilt LLVM/Clang release into $LLVM_HOME. This branch targets
# LLVM 8.0.0; the prebuilt Ubuntu 18.04 binary is what the Dockerfile pins, so
# there is no from-source LLVM build here (unlike the LLVM 3.4 branch).

set -e

if [ "giri$LLVM_HOME" == "giri" ]; then
	echo "Please set the LLVM_HOME env!"
	exit 1
fi

VERSION=8.0.0
[ $# -ge 1 ] && VERSION=$1

case $VERSION in
	"8.0.0")
		TARBALL=clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
		wget https://releases.llvm.org/8.0.0/$TARBALL
		mkdir -p $LLVM_HOME
		# The tarball unpacks into a single top-level dir; flatten it into
		# $LLVM_HOME so $LLVM_HOME/{bin,lib/cmake/llvm} are directly available.
		tar xf $TARBALL --strip-components=1 -C $LLVM_HOME
		rm -f $TARBALL
		;;
	*)
		echo "Unsupported LLVM version '$VERSION' for this branch (expected 8.0.0)."
		exit 1
		;;
esac
