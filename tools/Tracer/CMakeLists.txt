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

# The prebuilt LLVM 15.0.0 CMake package does not set the LLVM_COMPONENT_LIBS
# global property (only an in-tree LLVM build does), so
# llvm_map_components_to_libnames(TRACER_LLVM all) expands to an empty list
# and the executable fails to link with undefined LLVM references. Query the
# authoritative dependency-ordered library list from llvm-config and link by
# absolute path (--libfiles), because the package does not add the LLVM lib
# dir to the linker search path for bare -l names.
find_program(LLVM_CONFIG_EXE llvm-config HINTS "${LLVM_INSTALL_PREFIX}/bin")
execute_process(COMMAND "${LLVM_CONFIG_EXE}" --libfiles all
  OUTPUT_VARIABLE TRACER_LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE " " ";" TRACER_LLVM_LIBS "${TRACER_LLVM_LIBS}")
# LLVM's static libs reference dlopen/dlsym and pthread; the required system
# libs (from --system-libs) go after them on the link line.
execute_process(COMMAND "${LLVM_CONFIG_EXE}" --system-libs
  OUTPUT_VARIABLE TRACER_LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE " " ";" TRACER_LLVM_SYSTEM_LIBS "${TRACER_LLVM_SYSTEM_LIBS}")
target_link_libraries(tracer ${TRACER_LLVM_LIBS} ${TRACER_LLVM_SYSTEM_LIBS})