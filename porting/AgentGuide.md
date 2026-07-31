# AGENT-GUIDE.md

Detailed, portable build/test/debugging reference for agents working on any version branch.
Version-agnostic — does not reference a specific LLVM version.

## The CMake build directly (inside container)

```bash
mkdir -p build && cd build
cmake -DLLVM_DIR=$LLVM_HOME/lib/cmake/llvm ..
cmake --build . -- -j$(nproc)
cd ..
```

## Testing

Tests live under `test/` — plain Makefile-driven. Each test compiles C to LLVM bitcode
(`clang -g -O0 -emit-llvm` required for source-line slicing), instruments with `-trace-giri`,
runs the instrumented binary to produce a `.trace` file, then slices with `-dgiri` and diffs
against checked-in `ans-*.txt` golden output.

```bash
make -C test test                          # full suite (from auto-tests.txt)
cd test/UnitTests/test10 && make && make test  # single test
cd test/UnitTests/test10 && make rebuild && make clean  # clean/rebuild single test
```

`test/auto-tests.txt` is the authoritative list of test directories. Add new test dirs there
to include them in CI.

Each test Makefile sets `NAME`, `INPUT`, `CRITERION`, `TEST_ANS`, then includes
`test/Makefile.common` or `test/HelloWorld/Makefile` — read that shared Makefile before touching
test infra; it spells out the trace-then-slice pipeline end to end.

## Debugging

- **`-debug` flag** is wired through test Makefiles; stripped for `make test`.
- **`prtrace`** (`tools/PrintTrace/`) dumps a `.trace` file's `Entry` records in human-readable form.
- When debugging a slice that looks wrong, use `prtrace` and the `-debug` flag together.
- Build output: `build/lib/` (`.so`/`.a`) and `build/bin/` (executables) — flat, not nested.

## The opt invocation pipeline

The full pipeline (from `test/Makefile.common`):

```bash
# Step 1: Compile to whole-program IR
clang -g -O0 -c -emit-llvm file.c -o file.bc
llvm-link *.bc -o program.all.bc

# Step 2: Instrument
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum -trace-giri -trace-file=program.trace \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o program.trace.bc

# Step 3: Codegen + link
llc -O0 program.trace.bc -o program.trace.s
clang++ program.trace.s -o program.trace.exe -lrtgiri

# Step 4: Run (produces program.trace)
./program.trace.exe < real_input

# Step 5: Slice
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum \
    -dgiri -trace-file=program.trace -slice-file=program.slice \
    -criterion-loc=file.c:42 \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o /dev/null
```

## Key invariants recap

1. **Numbering determinism** — `-bbnum`/`-lsnum` must assign identical IDs whether run during
   instrumentation (step 2) or slicing (step 5) on equivalent IR.
2. **`Entry` struct ABI** — `Runtime.h` size must divide page size; `Tracing.cpp` (writer) and
   `TraceFile.cpp` (reader) must agree exactly.
3. **Debug info availability** — `-g` is mandatory for compile steps; debug info maps
   instructions back to `file:line`.