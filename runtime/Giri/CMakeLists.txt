##===- giri/runtime/Giri/CMakeLists.txt -------------------------------------===##
#
# librtgiri: the tracing runtime linked into instrumented programs (via
# `clang++ ... -lrtgiri`). This is plain C++/pthreads with NO LLVM dependency,
# so it needs none of LLVM's CMake helpers or include paths.
#
##===----------------------------------------------------------------------===##

add_library(rtgiri STATIC Tracing.cpp)

find_package(Threads REQUIRED)
target_link_libraries(rtgiri PUBLIC Threads::Threads)
