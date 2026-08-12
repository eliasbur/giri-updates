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

## Debugging

- **`-debug` flag** is wired through test Makefiles; stripped for `make test`.
- **`prtrace`** (`tools/PrintTrace/`) dumps a `.trace` file's `Entry` records in human-readable form.
- When debugging a slice that looks wrong, use `prtrace` and the `-debug` flag together.

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
