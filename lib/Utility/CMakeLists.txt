set(UTILITY_SRCS
  BasicBlockNumbering.cpp
  LoadStoreNumbering.cpp
  PostDominatorFrontier.cpp
  SourceLineMapping.cpp
  CountSrcLines.cpp
)

add_library(dgutility SHARED ${UTILITY_SRCS})

target_include_directories(dgutility PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Utility
  ${CMAKE_SOURCE_DIR}/include/Giri
  ${LLVM_INCLUDE_DIRS}
  )

# Allow unresolved symbols for `opt` to resolve at runtime
set_target_properties(dgutility PROPERTIES LINK_FLAGS "-Wl,--allow-shlib-undefined")