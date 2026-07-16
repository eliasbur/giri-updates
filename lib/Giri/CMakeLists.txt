##===- giri/lib/Giri/CMakeLists.txt -----------------------------------------===##
#
# The two core passes: TracingNoGiri (instrumentation, -trace-giri) and
# DynamicGiri (backward slicing, -dgiri), plus the trace-file reader.
#
# Compiled once as an OBJECT library so the same objects back both the loadable
# module libgiri.so (used via `opt -load`) and the standalone `tracer` tool.
#
##===----------------------------------------------------------------------===##

add_library(giri_obj OBJECT
  Giri.cpp
  TestPass.cpp
  TraceFile.cpp
  TracingNoGiri.cpp
)
# Objects feed a shared module, so they must be position-independent.
set_target_properties(giri_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Loadable module: libgiri.so. Symbols against LLVM are resolved by `opt` at
# load time, so no LLVM libraries are linked into the module itself.
add_library(giri MODULE $<TARGET_OBJECTS:giri_obj>)
set_target_properties(giri PROPERTIES LINKER_LANGUAGE CXX)
