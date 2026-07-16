##===- giri/tools/Tracer/CMakeLists.txt -------------------------------------===##
#
# `tracer`: a standalone opt-like driver that runs the tracing/slicing passes
# directly, for platforms without loadable-module support. It statically links
# the Giri/Utility pass objects and the LLVM components those passes need.
#
##===----------------------------------------------------------------------===##

add_executable(tracer
  Tracer.cpp
  $<TARGET_OBJECTS:giri_obj>
  $<TARGET_OBJECTS:dgutility_obj>
)

llvm_map_components_to_libnames(tracer_llvm_libs
  bitreader bitwriter instrumentation scalaropts ipo nativecodegen
)
target_link_libraries(tracer PRIVATE ${tracer_llvm_libs})
