##===- giri/tools/PrintTrace/CMakeLists.txt ---------------------------------===##
#
# `prtrace`: dumps a .trace file's Entry records in human-readable form.
#
##===----------------------------------------------------------------------===##

add_executable(prtrace PrintTrace.cpp)

llvm_map_components_to_libnames(prtrace_llvm_libs support)
target_link_libraries(prtrace PRIVATE ${prtrace_llvm_libs})
