# The prebuilt LLVM CMake package does not set the LLVM_COMPONENT_LIBS global
# property (only an in-tree LLVM build does), so
# llvm_map_components_to_libnames(TRACER_LLVM all) expands to an empty list
# and the executable fails to link with undefined LLVM references. Query the
# authoritative dependency-ordered library list from llvm-config and link by
# absolute path (--libfiles), because the package does not add the LLVM lib
# dir to the linker search path for bare -l names. (Inherited from the 15.0.0
# port; the 16.0.0 prebuilt's package behaves the same way.)
find_program(LLVM_CONFIG_EXE llvm-config HINTS "${LLVM_INSTALL_PREFIX}/bin")
execute_process(COMMAND "${LLVM_CONFIG_EXE}" --libfiles all
  OUTPUT_VARIABLE TRACER_LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE " " ";" TRACER_LLVM_LIBS "${TRACER_LLVM_LIBS}")
# LLVM's static libs reference dlopen/dlsym and pthread; the required system
# libs (from --system-libs) go after them on the link line.
execute_process(COMMAND "${LLVM_CONFIG_EXE}" --system-libs
  OUTPUT_VARIABLE TRACER_LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE " " ";" TRACER_LLVM_SYSTEM_LIBS "${TRACER_LLVM_SYSTEM_LIBS}")

# GLIBCXX interop shim (see llvm_std_shim.cpp): the prebuilt's *static* LLVM
# libs reference std::__throw_bad_array_new_length() only when the prebuilt was
# built with a libstdc++ newer than the host's — true of the 15.0.0
# x86_64-linux-gnu-rhel-8.4 prebuilt (GCC >= 10). The 16.0.0 prebuilt
# (x86_64-linux-gnu-ubuntu-18.04, built with GCC 7.5) does not reference the
# symbol at all, and ubuntu:18.04's libstdc++ lacks the _GLIBCXX_NODISCARD
# macro the shim TU uses, so compiling it there fails. Rather than guess by
# version, probe each static LLVM lib at configure time and compile the shim TU
# only when the symbol is actually referenced (verified: no 16.0.0 static lib
# references it; the 15.0.0 rhel-8.4 libLLVMAnalysis.a / libLLVMBitWriter.a do).
find_program(NM_EXE nm)
set(TRACER_NEEDS_GLIBCXX_SHIM OFF)
if(NM_EXE AND NOT WIN32 AND NOT APPLE)
  foreach(_lib ${TRACER_LLVM_LIBS})
    if(_lib MATCHES "\\.a$")
      execute_process(COMMAND "${NM_EXE}" -C "${_lib}"
        OUTPUT_VARIABLE _lib_syms ERROR_QUIET)
      if(_lib_syms MATCHES "__throw_bad_array_new_length")
        set(TRACER_NEEDS_GLIBCXX_SHIM ON)
      endif()
    endif()
  endforeach()
endif()

set(_tracer_srcs Tracer.cpp)
if(TRACER_NEEDS_GLIBCXX_SHIM)
  list(APPEND _tracer_srcs llvm_std_shim.cpp)
  message(STATUS "tracer: prebuilt static LLVM libs reference std::__throw_bad_array_new_length(); compiling llvm_std_shim.cpp")
else()
  message(STATUS "tracer: prebuilt static LLVM libs reference no missing GLIBCXX symbol; llvm_std_shim.cpp not needed")
endif()
add_executable(tracer ${_tracer_srcs})

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
target_link_libraries(tracer ${TRACER_LLVM_LIBS} ${TRACER_LLVM_SYSTEM_LIBS})
