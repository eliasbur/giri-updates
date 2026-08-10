add_library(rtgiri STATIC
  Tracing.cpp
)

target_include_directories(rtgiri PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Giri
  )

target_link_libraries(rtgiri PRIVATE pthread)
set_target_properties(rtgiri PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )