//===- llvm_std_shim.cpp - toolchain interop shim for the 15.0.0 prebuilt
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The 15.0.0 prebuilt (x86_64-linux-gnu-rhel-8.4) was built with a GCC whose
// libstdc++ is newer than the host (ubuntu:20.04 ships gcc 9.4,
// libstdc++ 6.0.28 = GLIBCXX_3.4.28). Two of the prebuilt's *static* LLVM
// libraries (libLLVMAnalysis.a, libLLVMBitWriter.a) reference
// `std::__throw_bad_array_new_length()` (mangled
// `_ZSt28__throw_bad_array_new_lengthv`), a helper first defined in
// GLIBCXX_3.4.29 (GCC 10+). The host libstdc++ exports the *class*
// `std::bad_array_new_length` (added in GCC 9) but not that helper
// *function*, so a *static* link of the LLVM libs into `tracer` fails with
// `undefined reference to std::__throw_bad_array_new_length()`.
//
// The prebuilt *binaries* (opt/clang) are shared and only lazily resolve the
// symbol, and the slicing pipeline never hits the `new[]` length-throw path,
// so they run fine on the host libstdc++. Only `tracer` — the one executable
// that statically links `llvm-config --libfiles all` — needs the symbol at
// *link* time. This TU defines it, matching the upstream libstdc++ body
// (throw the exception object). `libgiri.so`/`libdgutility.so`/`prtrace` do
// not statically link the LLVM static libs (they use --allow-shlib-undefined
// / shared linkage), so only the `tracer` target needs this object.
//
// Defining a function in namespace `std` is formally ill-formed, but this is
// a deliberate, minimal ABI shim to link against a prebuilt built with a
// newer toolchain; the symbol is otherwise absent, so there is no ODR clash.
//
//===----------------------------------------------------------------------===//

#include <new> // std::bad_array_new_length

namespace std {

_GLIBCXX_NODISCARD bad_array_new_length __throw_bad_array_new_length() {
  throw bad_array_new_length();
}

} // namespace std
