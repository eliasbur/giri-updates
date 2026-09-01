# Cold-build + negative-control acceptance (16.0.0 new-PM port)

- `cold-build-suite.log` — `/giri/build` wiped, then `source /giri/utils/build.sh`
  (fresh CMake configure with the 16.0.0 toolchain + full rebuild + the honest
  `TEST_PARALLELISM=seq` suite). Result: build.sh exit 0, **22 tests PASS**,
  five artifacts in `build/{lib,bin}`.
- `negative-control-and-provenance.log` — toolchain provenance (os/glibc/gcc/
  cmake/llvm-config/clang, the GitHub-Releases prebuilt asset, clean `ldd`,
  GLIBCXX max-symbol check, `libtinfo.so.5`, `--cxxflags` = `-std=c++17`,
  no CMake floor in the 16.0.0 prebuilt package) plus the negative control
  (an empty slice must NOT diff-equal a non-empty golden).
