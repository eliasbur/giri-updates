add_executable(prtrace PrintTrace.cpp)

target_include_directories(prtrace PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Giri
  ${CMAKE_SOURCE_DIR}/include/Utility
  ${LLVM_INCLUDE_DIRS}
  )

llvm_map_components_to_libnames(PRTRACE_LLVM support core)
target_link_libraries(prtrace ${PRTRACE_LLVM})