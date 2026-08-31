# Source this before running ARVO's `compile`.  It redirects the build through
# the bitcode-capturing wrappers and strips everything that exists only to serve
# libFuzzer + ASan, which a Giri trace run neither needs nor tolerates.
export GIRI_ROOT="${GIRI_ROOT:-/giri}"
export GIRI_BC_DIR="$GIRI_ROOT/bc";        mkdir -p "$GIRI_BC_DIR"

# Wrappers.  GIRI_REAL_* are absolute so the wrapper still works if it is also
# installed as `clang`/`clang++` on PATH for build systems that ignore $CC.
export GIRI_REAL_CC=/usr/local/bin/clang
export GIRI_REAL_CXX=/usr/local/bin/clang++
export CC="$GIRI_ROOT/bin/giri-clang"
export CXX="$GIRI_ROOT/bin/giri-clang++"
export PATH="$GIRI_ROOT/bin:$PATH"

# No ASan: its handlers _exit() on the fault, which would truncate the trace,
# and its instrumentation would end up inside the bitcode Giri slices.
export SANITIZER_FLAGS_address=""
export SANITIZER_FLAGS_undefined=""
export SANITIZER_FLAGS_memory=""
# No SanitizerCoverage: those callbacks live in libFuzzer, which we are removing.
export COVERAGE_FLAGS=""

# Replace libFuzzer's driver with a single-input main() (see compile_giri).
export FUZZING_ENGINE=giri

# Keep the container's original /out reproduction artifacts intact.
export OUT="$GIRI_ROOT/out";               mkdir -p "$OUT"
