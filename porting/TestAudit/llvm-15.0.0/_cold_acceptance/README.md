# Cold-build + negative-control acceptance (LLVM 15.0.0 new-PM port)

Supplemental evidence for `SUMMARY.md`: a **from-scratch** build of the final
committed source at HEAD, a full re-run of the suite and the standalone-tracer
validation from that cold build, and a **negative control** proving the golden
match is not a false positive. Run in the `giri15` container (image
`giri-llvm-15`, source synced to HEAD) on 2026-08-28.

## 1. Cold from-scratch build at HEAD

`/giri/build` wiped, then `source /giri/utils/build.sh` (cmake configure +
`make -j` + `make -C test`). The source in the container was synced to HEAD
first (the one file that differed, `tools/Tracer/Tracer.cpp`, was a comment
only; after sync its md5 matched host HEAD).

Result (`cold-build-suite.log`): **build.sh exit 0**, all **22 tests PASS**
(19 UnitTests test1–5, test8–21 + matrix_multiply/pca/kmeans seq), five
artifacts produced in `build/{lib,bin}` (`libgiri.so`, `libdgutility.so`,
`librtgiri.a`, `tracer`, `prtrace`). This is the packaging acceptance path the
incremental 22/22 did not exercise: a clean configure+build+test from the
committed tree.

## 2. Standalone `tracer` from the cold build

`cold-standalone-tracer.log`: driving the cold-built `tracer` binary over all
22 test cases (harness's exact inputs/criteria/LDFLAGS) → **FULL RESULT: 22
PASS / 0 FAIL (of 22)**; `prtrace` OK on every trace. Same result as the
committed validation, now reproduced from a cold build.

## 3. Negative control (failure mode): missing trace must not match the golden

A golden match is only meaningful if a *missing* trace cannot reproduce it.
In `test/UnitTests/test1`: after a normal instrument+run (trace file present,
4-line slice == golden `9 12 13 18`), the trace file was **removed** (keeping
the `.bc`) and the `dgiri` slice re-run:

- `opt … -passes="…dgiri…" -trace-file=extlibcalls.trace …` → **rc 139
  (SIGSEGV)**, the `DynamicGiriPass` aborts in the `TraceFile` ctor path.
  The `(fd > 0) && "Cannot open file!"` assert at `lib/Giri/TraceFile.cpp:52`
  is compiled out in the Release build (`CMAKE_BUILD_TYPE=Release`), so with
  the prebuilt Release toolchain a missing trace is a silent null-deref/segfault,
  not a catchable assert.
- **`extlibcalls.slice` is ABSENT after the crash** → there is nothing to diff
  against the golden, so a false golden match is impossible without a
  genuinely written+read trace.
- Positive control: restoring the real trace and re-running through the honest
  harness (`make` then `make test`) → diff empty, slice `9 12 13 18` == golden,
  exit 0.

This mirrors the 14.0.0-newpm negative control (commit `9a2a29e`) and
confirms the 15.0.0 `tracer`/`dgiri` path has the same "no trace ⇒ no match"
guarantee.
