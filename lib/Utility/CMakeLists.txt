set(UTILITY_SRCS
  BasicBlockNumbering.cpp
  LoadStoreNumbering.cpp
  PostDominatorFrontier.cpp
  SourceLineMapping.cpp
  CountSrcLines.cpp
  UtilityPassPlugin.cpp
)

add_library(dgutility SHARED ${UTILITY_SRCS})

target_include_directories(dgutility PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Utility
  ${CMAKE_SOURCE_DIR}/include/Giri
  ${LLVM_INCLUDE_DIRS}
  )