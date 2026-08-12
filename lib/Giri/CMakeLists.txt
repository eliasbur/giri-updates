set(GIRI_SRCS
  Giri.cpp
  TracingNoGiri.cpp
  TraceFile.cpp
  TestPass.cpp
)

add_library(giri SHARED ${GIRI_SRCS})

target_include_directories(giri PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Giri
  ${CMAKE_SOURCE_DIR}/include/Utility
  ${LLVM_INCLUDE_DIRS}
  )

set_target_properties(giri PROPERTIES LINK_FLAGS "-Wl,--allow-shlib-undefined")
target_link_libraries(giri dgutility)