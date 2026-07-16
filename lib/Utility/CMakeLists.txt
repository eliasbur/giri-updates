##===- giri/lib/Utility/CMakeLists.txt --------------------------------------===##
#
# Supporting analysis passes (BasicBlock/LoadStore numbering, post-dominance
# frontier, source-line mapping, source-line counting).
#
# Compiled once as an OBJECT library so the same objects back both the loadable
# module libdgutility.so (used via `opt -load`) and the standalone `tracer` tool.
#
##===----------------------------------------------------------------------===##

add_library(dgutility_obj OBJECT
  BasicBlockNumbering.cpp
  CountSrcLines.cpp
  LoadStoreNumbering.cpp
  PostDominatorFrontier.cpp
  SourceLineMapping.cpp
)
# Objects feed a shared module, so they must be position-independent.
set_target_properties(dgutility_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Loadable module: libdgutility.so. Symbols against LLVM are resolved by `opt`
# at load time, so no LLVM libraries are linked into the module itself.
add_library(dgutility MODULE $<TARGET_OBJECTS:dgutility_obj>)
set_target_properties(dgutility PROPERTIES LINKER_LANGUAGE CXX)
