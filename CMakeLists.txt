cmake_minimum_required(VERSION 3.5)
project(Giri C CXX)

find_package(LLVM 15.0 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

# Allow unresolved symbols in .so files (resolved by `opt` at load time)
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--allow-shlib-undefined" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--allow-shlib-undefined")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

add_compile_options(${LLVM_OPTIMIZED_CXX_FLAGS})
if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(lib/Utility)
add_subdirectory(lib/Giri)
add_subdirectory(runtime/Giri)
add_subdirectory(tools/Tracer)
add_subdirectory(tools/PrintTrace)