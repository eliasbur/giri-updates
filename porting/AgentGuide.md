# AGENT-GUIDE.md

Detailed, portable build/test/debugging reference for agents working on any version branch.
Version-agnostic — does not reference a specific LLVM version.

**All commands below must be run inside the Giri Docker container, never from the agent devcontainer.**

## Building

Use the convenience script (rebuilds modified parts only):

```bash
source /giri/utils/build.sh
```

Build output is flat: `build/lib` (`.so`/`.a`) and `build/bin` (executables).
The script runs the full test suite automatically after building. 

## Testing

Tests live under `test/` — plain Makefile-driven. Each test compiles C to LLVM bitcode
(`clang -g -O0 -emit-llvm` required for source-line slicing), instruments with `-trace-giri`,
runs the instrumented binary to produce a `.trace` file, then slices with `-dgiri` and diffs
against checked-in `ans-*.txt` golden output.

```bash
make -C /giri/test                          # full suite (from auto-tests.txt)
cd /giri/test/UnitTests/test10 && make && make test  # single test
cd /giri/test/UnitTests/test10 && make rebuild && make clean  # clean/rebuild single test
```

`test/auto-tests.txt` is the authoritative list of test directories. Add new test dirs there
to include them in CI.

Each test Makefile sets `NAME`, `INPUT`, `CRITERION`, `TEST_ANS`, then includes
`test/Makefile.common` or `test/HelloWorld/Makefile` — read that shared Makefile before touching
test infra; it spells out the trace-then-slice pipeline end to end.

### Test harness — per-test logs and failure reporting

Every pipeline stage's stdout/stderr is captured to `test/_test_logs/<flat-name>.log`
(e.g. `UnitTests_test5.log`). The suite prints one `[PASS]`/`[FAIL]` line per test. Both build
failures (compiler errors, `opt` crashes) and non-zero exits from traced binaries cause
`[FAIL]`.

- **Consolidated log:** run with `make -C test TEST_LOG=test-suite.log` to collect all per-test
  logs into one file after the suite finishes.
- **Inspect a single failure:** `cat test/_test_logs/<name>.log` shows build + test output.
- **Quiet default:** with no `TEST_LOG` set, the harness stays quiet and only prints the
  one-line-per-test table.
- **Cleanup:** `make clean -C test` removes `_test_logs/` along with build artefacts.

### Diagnosing new failures under the honest harness

The harness now surfaces failures that were previously hidden by error suppression. When tests
that passed under the old suppressive harness now fail, look at the per-test log for the stage
that failed:
- `make[1]: *** [<name>.trace] Error N` means the traced binary exited with code `N`.
- Compiler errors and `opt` crashes appear in the build portion of the log.
- Per-test logs include stage markers (`=== STAGE: BUILD ===`, `=== STAGE: TEST ===`) to
  distinguish build output from test output.
- A failing test leaves its artifacts (`.bc`, `.trace`, `.trace.bc`) behind for post-mortem;
  only passing tests are cleaned.

### Declaring an acceptable exit status

Some traced programs naturally return non-zero from `main()`. The harness supports two
per-test declarations in the test's Makefile (before `include ../../Makefile.common`):

- **`EXPECTED_EXIT = N`** — the traced binary is expected to exit with code `N`. The
  Makefile checks the actual exit and only proceeds if it matches. Add a commented one-liner
  explaining the derivation, e.g. `# fibonacci(15) = 610 → 610 & 0xFF = 98`.
- **`EXIT_UNCHECKED = 1`** — the exit code is inherently unpredictable (undefined behaviour,
  platform-dependent). Set this only when `EXPECTED_EXIT` cannot be used; the trace is still
  generated and sliced, and program output is preserved in the per-test log. Only exit statuses
  below 128 are ignored.

With no declaration (default), a non-zero exit from the traced binary causes `[FAIL build]`.
An `opt` crash always produces `[FAIL]`. A deliberately wrong `EXPECTED_EXIT` also produces
`[FAIL build]` with a diagnostic message.

> **A traced binary never dies by signal, so `128 + n` never appears.** `recordInit`
> (`runtime/Giri/Tracing.cpp:272-280`) installs `cleanup_only_tracing` for SIGSEGV, SIGABRT,
> SIGINT, SIGQUIT, SIGTERM, SIGILL and SIGFPE, and that handler ends in `exit(signum)`
> (`Tracing.cpp:253-256`). A segfaulting traced program therefore exits **11**, an aborting one
> **6** — normal exits, indistinguishable by status alone from a `main()` that returned the same
> number.
>
> The trace recipe (`test/Makefile.common`) captures stderr to a temp file, checks for the marker
> `[GIRI] Abnormal termination`, and fails the stage if found. This catches crashes under both
> `EXIT_UNCHECKED=1` and `EXPECTED_EXIT = N`, regardless of exit code. stderr is passed through via
> `cat` so it appears in the per-test log.
>
> Under `EXPECTED_EXIT = N`, avoid declaring an `N` that is also a handled signal number
> (2, 3, 4, 6, 8, 11, 15) without saying why — the [GIRI] marker catches the crash, but the
> collision is still worth noting. Comments documenting the collision are in-place for the four
> tests that declare `EXPECTED_EXIT = 2` (SIGINT).

## Debugging

- **`DEBUGFLAGS`** in test Makefiles controls extra `opt` flags (set to empty string for LLVM 5+).
- **`prtrace`** (`tools/PrintTrace/`) dumps a `.trace` file's `Entry` records in human-readable form.
- When debugging a slice that looks wrong, use `prtrace` to inspect the trace file.
- **-debug/-debug-only= are inert** on the no-asserts 5.0.2 prebuilt toolchain — `opt` ignores them. Giri's own `assert()`s are also compiled out by the `Release` CMake build. Note that test programs' `assert()`s (from C source) still fire through libc — that is how `kmeans-pthread` aborts on many-CPU hosts.

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
