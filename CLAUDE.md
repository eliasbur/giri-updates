# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Giri is a dynamic program slicing tool implemented as an out-of-tree LLVM 3.4 project. Given a
specific execution of a program and a "slicing criterion" (an instruction/value), it computes the
*dynamic backwards slice*: the set of instructions from that execution that actually influenced the
criterion's value. It works in two phases:

1. **Tracing**: an LLVM pass instruments a bitcode module so that, at runtime, it logs every basic
   block entry, load, store, call, return, and select to a binary trace file.
2. **Slicing**: a separate LLVM analysis pass reads the trace file back and walks it (combined with
   LLVM's static def-use/SSA graph and post-dominance info) to compute the backwards slice from a
   given criterion, mapping the result back to source line numbers.

**This branch (`port/llvm-8.0.0`) is porting Giri from LLVM 3.4 to LLVM/Clang 8.0.0.** The original
project targeted LLVM 3.4 with LLVM's old autoconf+Makefile build; on this branch the build system is
now **CMake** and the target toolchain is **LLVM/Clang 8.0.0**.

**Port status:** the *foundation* is landed — CMake build system, mechanical header/rename fixes, and
`DataLayout`-as-a-pass removal (PORTING.md Phases 1–3). Still **not done**: the Phase 0 spike and the
three semantic redesigns — the Metadata/Value split in `BasicBlockNumbering`/`LoadStoreNumbering`
(Phase 4), debug-info line mapping (Phase 5), and `PostDominanceFrontier` (Phase 6). Until those land,
the CMake build compiles past the mechanical sites and then fails at the Phase 4/5/6 code — that is
expected. See [`PORTING.md`](PORTING.md) for the phased plan and follow its sequencing; if asked to
continue the port, read it first rather than improvising.

For a deep dive into the actual tracing/slicing pipeline — the LLVM pass invocations, the on-disk
trace format, and the invariants a change (or the port) must preserve — see
[`HOW_IT_WORKS.md`](HOW_IT_WORKS.md).

## Build

Giri is built and tested **only inside Docker** on this branch — never on the host. The `Dockerfile`
pins Ubuntu 18.04 + prebuilt LLVM/Clang 8.0.0 (the release LLVM's 8.0.0 binaries target):

```bash
docker build -t giri-llvm8 .
```

This runs `utils/install_llvm.sh 8.0.0` (downloads the prebuilt LLVM 8.0.0 release into `$LLVM_HOME`)
followed by `utils/build.sh` (CMake-configures and builds Giri against it via `$LLVM_DIR`, then runs
the test suite) inside the container — see [`Dockerfile`](Dockerfile) for the exact steps.

Because the LLVM toolchain is an early, cached Docker layer, don't rebuild the image for every code
change: once the base image exists, `docker run` a container and iterate (`cmake --build build`, rerun
tests) inside it. This is also how the remaining port phases (4–6) should be worked, since the full
image build will fail at those still-unported sites until they're done.

### The CMake build directly (inside the container, or against any LLVM 8.0.0 install)

```bash
cmake -S . -B build -DLLVM_DIR=$LLVM_HOME/lib/cmake/llvm
cmake --build build -j
```

Build output lands in a **flat** `build/lib` (`libgiri.so`, `libdgutility.so`, `librtgiri.a`) and
`build/bin` (`tracer`, `prtrace`) — not the old autoconf `build/<BuildMode>/{lib,bin}` layout. The
`test/` Makefiles (`test/Makefile.common`, `test/HelloWorld/Makefile`) point at `build/lib` and
`build/bin` accordingly.

## Testing

Tests live under `test/` and are plain Makefile-driven: each test dir compiles a C program to LLVM
bitcode with `clang -O0 -emit-llvm`, links it, runs the tracing pass + instrumented binary to produce
a `.trace` file, then runs the `-dgiri` slicing pass against a criterion (source line or instruction
number) and diffs the resulting sliced source lines against a checked-in `ans-*.txt` file.

```bash
# Run the full test suite (from repo root, after building) — matches CI (.travis.yml)
make -C test test

# Run a single test
cd test/UnitTests/test10   # or test/matrix_multiply, test/pca, test/kmeans, test/HelloWorld, ...
make
make test                  # diffs computed slice against ans-inst.txt / criterion-*.txt

# Rebuild a test from clean
make rebuild
make clean
```

`test/auto-tests.txt` is the authoritative list of test directories the top-level `make -C test test`
target iterates over — add new test dirs there to have them included in CI. Each test's own Makefile
sets `NAME`, `INPUT`, `CRITERION`, and `TEST_ANS`, then includes the shared
`test/UnitTests/Makefile.common` (or `test/HelloWorld/Makefile`'s equivalent) which defines the actual
`opt`/`llc`/`clang` build+trace+slice pipeline. Read that shared Makefile before touching test infra —
it's the one place the trace-then-slice pipeline is spelled out end to end.

## Code layout

- `include/Giri/`, `lib/Giri/` — the two core LLVM passes:
  - `TracingNoGiri` (`Giri.h`/`TracingNoGiri.cpp`) — a `BasicBlockPass` + `InstVisitor` that inserts
    calls into the runtime tracing library (`RecordLoad`, `RecordStore`, `RecordBB`, `RecordCall`,
    `RecordSelect`, ...) for every relevant instruction. Registered under `-trace-giri`.
  - `DynamicGiri` (`Giri.h`/`Giri.cpp`) — a `ModulePass` that reads back the trace via `TraceFile` and
    computes the backwards slice (`getBackwardsSlice`/`findSlice`), using `PostDominatorTree` /
    `PostDominanceFrontier` for control dependence. Registered under `-dgiri`, with flags
    `-criterion-loc` / `-criterion-inst` to pick the slicing criterion and `-slice-file` for output.
  - `TraceFile.h`/`TraceFile.cpp` (largest file, ~1500 lines) — parses/`mmap`s the binary trace file
    and implements the trace-scanning search logic (`findPreviousID`, `findAllStoresForLoad`,
    `getSourcesFor*`, etc.) that reconstructs dynamic data/control flow from the log. This is the most
    subtle and bug-prone part of the codebase (see recent commit history for an example fix in
    `findAllStoresForLoad`) — changes here need care around trace indices, thread IDs, and
    partial-overlap load/store matching.
  - `Runtime.h` — defines the on-disk `Entry` struct (the trace record format) shared between the
    instrumentation-time runtime and the slicing-time reader. Its size must stay a divisor of both the
    page size and the runtime's in-memory cache size — see the warning comment in that file before
    adding/removing fields.
- `include/Utility/`, `lib/Utility/` — supporting LLVM analysis passes used by both Giri passes:
  `BasicBlockNumbering` / `LoadStoreNumbering` (assign stable numeric IDs to BBs and load/store insts,
  used as trace record IDs), `PostDominanceFrontier`, `SourceLineMapping` (maps instructions back to
  source file:line for slice reporting), `CountSrcLines`.
- `runtime/Giri/Tracing.cpp` — the actual runtime library (`librtgiri`) linked into instrumented
  binaries; implements the `Record*`/`Init` functions that `TracingNoGiri` calls, writing `Entry`
  records to the trace file. Built as `ARCHIVE_LIBRARY`/`BYTECODE_LIBRARY` (see `runtime/Giri/Makefile`).
- `tools/Tracer/` — standalone `opt`-like driver that runs the tracing pass on a bitcode file directly
  (useful on platforms without shared library support, e.g. the Cygwin comment in `Tracer.cpp`).
- `tools/PrintTrace/` — `prtrace`, a small utility that dumps a `.trace` file's `Entry` records in
  human-readable form; useful for debugging trace content directly.

## Working in this codebase

- This is LLVM 3.4-vintage C++ (pre-`std::unique_ptr`-everywhere, uses `llvm::OwningPtr` idioms in
  places, old pass-manager APIs like `RegisterPass`/`INITIALIZE_PASS`, `PassManager` not the new pass
  manager). Match existing patterns rather than introducing modern LLVM/C++ idioms that don't exist in
  this LLVM version.
- The two passes communicate only through the on-disk trace file and the `BasicBlockNumbering`/
  `LoadStoreNumbering` ID scheme — IDs must be assigned identically at trace time and slice time, which
  is why both passes require those numbering passes via `getAnalysisUsage`.
- When debugging a slice that looks wrong, `prtrace` (`tools/PrintTrace`) and the `-debug` flag (wired
  through `DEBUGFLAGS` in the test Makefiles) are the primary tools — the test Makefiles pass
  `-debug` by default and only strip it for `make test`.
