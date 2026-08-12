add_executable(tracer Tracer.cpp)

target_include_directories(tracer PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/Giri
  ${CMAKE_SOURCE_DIR}/include/Utility
  ${LLVM_INCLUDE_DIRS}
  )

# Tracer needs exceptions (uses try/catch)
set_target_properties(tracer PROPERTIES
  COMPILE_FLAGS "-fexceptions"
  LINK_FLAGS "-fexceptions"
)

target_link_libraries(tracer giri dgutility)

llvm_map_components_to_libnames(TRACER_LLVM all)
target_link_libraries(tracer ${TRACER_LLVM})