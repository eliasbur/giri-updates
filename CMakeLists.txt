##===- giri/CMakeLists.txt --------------------------------------------------===##
#
# Out-of-tree CMake build for Giri against LLVM/Clang 8.0.0.
#
# This replaces the original LLVM-3.4 autoconf/Make build. Point CMake at an
# LLVM 8.0.0 install with -DLLVM_DIR=<llvm-install>/lib/cmake/llvm. See the
# Dockerfile for the canonical (Ubuntu 18.04 + LLVM 8.0.0) build environment;
# Giri is meant to be built inside that container, not on the host.
#
##===----------------------------------------------------------------------===##

cmake_minimum_required(VERSION 3.4.3)
project(Giri CXX C)

# Locate LLVM 8.0.x. In the Docker image LLVM_DIR is exported for us; on a manual
# build pass -DLLVM_DIR=<llvm-install>/lib/cmake/llvm.
find_package(LLVM 8.0 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

# Apply the same preprocessor/definitions the LLVM headers expect, and put both
# the LLVM headers and Giri's own include/ on the search path.
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The passes must match LLVM's RTTI setting or they won't load/link. The stock
# LLVM 8.0.0 release is built without RTTI.
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

# Flat output layout the test harness points at: build/lib and build/bin
# (no Release+Asserts mode subdirectory like the old Make build used).
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Utility/Giri must be configured before tools/Tracer, which links their objects.
add_subdirectory(lib/Utility)
add_subdirectory(lib/Giri)
add_subdirectory(runtime/Giri)
add_subdirectory(tools/Tracer)
add_subdirectory(tools/PrintTrace)
