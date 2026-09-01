# LLVM 16.0.0 port audit (new pass manager)

Port of Giri to **LLVM 16.0.0**, **new pass manager**, cut from the completed
15.0.0 new-PM port head `63b02e2` (PR #21). No legacy pass manager anywhere;
every `opt` invocation runs a `-passes="…"` pipeline and each Giri library is
loaded twice (`-load` registers the plugin's `cl::opt` globals before `opt`
parses the command line; `-load-pass-plugin` registers the `-passes` names
after).

**Bottom line:** the honest `TEST_PARALLELISM=seq` suite reports **22 PASS / 0
FAIL** (rc=0) against the pristine 3.4 goldens — 19 UnitTests (test1–5,
test8–21) plus the three app benchmarks (matrix_multiply, pca, kmeans) in their
seq variant. The **standalone `tracer`** binary (not `opt`) over the same 22
test cases is also **22 PASS / 0 FAIL** (the DataLayout self-assignment fix
inherited from the 15.0.0 port keeps it clean). All three critical invariants
hold: numbering determinism, the `Entry` struct ABI (`sizeof(Entry)=32`,
`4096 % 32 == 0`), and `-g` debug-info slicing.

The substantive risk this port carried — opaque pointers — was **disproven to
be a risk** (see "Root-cause fixes", below): the 15.0.0 harness's `-Xclang
-no-opaque-pointers` passthrough flag is **honored** by the 16.0.0
ubuntu-18.04 prebuilt clang and is **load-bearing** (typed IR with it, opaque
`ptr` without). No harness change was needed for 16.0.0, so
`git diff 63b02e2..HEAD -- test/` is empty (stronger than the 15.0.0 port's
"limited to the 2 harness files").

Evidence lives alongside this file: the suite's raw per-test stage logs under
`_test_logs/*.log`, the standalone-tracer validation under `_tool_validation/`
(`full_tool_validation.py`, `full-tool-validation.txt`), and the cold-build +
negative-control + toolchain-provenance acceptance under `_cold_acceptance/`
(`cold-build-suite.log`, `negative-control-and-provenance.log`). The 22
per-test reports (`UnitTests-testN.md` / `<bench>-seq.md`) each carry the
golden name and line count, the input, the criterion actually used, the diff
status, and the captured stage reference.

## Suite results (22 PASS / 0 FAIL)

All 22 rows `PASS` (honest `TEST_PARALLELISM=seq`; see `suite_final_table.txt`
equivalent — the tallies below). Every diff is empty: the harness's
`diff $(NAME).slice.loc $(TEST_ANS)` exits 0, i.e. the 16.0.0 `file:line`
slice is byte-identical to the pristine 3.4-era golden. Golden/criterion
values are the source-tree ground truth (`ans-*.txt` + `criterion-*.txt` under
each test directory); "implicit" means the test's Makefile passes no explicit
`-criterion-*` flag.

| Test | Golden (lines) | Criterion | Diff vs 3.4 golden |
|------|----------------|-----------|--------------------|
| test1  | ans-inst.txt (4)   | implicit      | empty |
| test2  | ans-inst.txt (4)   | implicit      | empty |
| test3  | ans-inst.txt (9)   | implicit      | empty |
| test4  | ans-inst.txt (7)   | `criterion-loc` | empty |
| test5  | ans-inst.txt (12)  | implicit      | empty |
| test8  | ans-inst.txt (4)   | implicit      | empty |
| test9  | ans-inst.txt (5)   | implicit      | empty |
| test10 | ans-inst.txt (4)   | implicit      | empty |
| test11 | ans-inst.txt (7)   | implicit      | empty |
| test12 | ans-inst.txt (20)  | implicit      | empty |
| test13 | ans-inst.txt (6)   | implicit      | empty |
| test14 | ans-inst.txt (7)   | implicit      | empty |
| test15 | ans-inst.txt (12)  | implicit      | empty |
| test16 | ans-inst.txt (7)   | implicit      | empty |
| test17 | ans-inst.txt (6)   | implicit      | empty |
| test18 | ans-inst.txt (4)   | implicit      | empty |
| test19 | ans-inst.txt (10)  | implicit      | empty |
| test20 | ans-inst.txt (11)  | implicit      | empty |
| test21 | ans-inst.txt (6)   | implicit      | empty |
| matrix_multiply | ans-inst-seq.txt (19) | `criterion-inst-seq.txt` | empty |
| pca           | ans-inst-seq.txt (17) | `criterion-inst-seq.txt` | empty |
| kmeans        | ans-inst-seq.txt (2)  | `criterion-inst-seq.txt` | empty |

## Standalone-tracer cross-check (22 PASS / 0 FAIL)

`_tool_validation/` re-runs the 22 test cases driving the `tracer` binary
(which builds its pipeline programmatically with the new PM and a hand-built
`ModuleAnalysisManager`) instead of `opt`. Result: **22 PASS / 0 FAIL**
(`full-tool-validation.txt` records `standalone-tracer FULL RESULT: 22 PASS /
0 FAIL`). kmeans (the test the 15.0.0 port once saw drift under opaque IR)
matches its 2-loc golden cleanly here, and `prtrace` decodes every trace
(Entry-ABI public reader), confirming the trace record format is intact.

## Cold-build + negative-control acceptance

`_cold_acceptance/` re-establishes the result from a **clean** configure+build
of the final committed source:

- **Cold from-scratch build at HEAD** — `/giri/build` wiped, then
  `source /giri/utils/build.sh`. Result (`cold-build-suite.log`): build.sh exit
  0, all **22 tests PASS**, five artifacts in `build/{lib,bin}`.
- **Negative control** — a non-empty golden (test1, 4 numeric lines) diffed
  against an empty slice **fails** (diff rc=1), proving the 22/22 match is not
  a degenerate "both empty" pass.
- **Toolchain provenance** (`negative-control-and-provenance.log`):
  `ubuntu:18.04.6` (glibc 2.27, gcc 7.5.0), CMake 3.31.12,
  `llvm-config --version` 16.0.0 (Release), `clang version 16.0.0`
  (x86_64-linux-gnu), `--cxxflags` shows `-std=c++17`. The prebuilt
  (GitHub Releases `llvmorg-16.0.0`, `clang+llvm-16.0.0-…-ubuntu-18.04.tar.xz`)
  has a **clean `ldd`** on opt/clang (no missing `libtinfo.so.5` on 18.04,
  unlike 20.04), and the max GLIBCXX symbol it needs (3.4.21) ≤ the 18.04 host
  libstdc++ provides (3.4.25) → **no link shim**.

## Root-cause fixes (15.0.0 → 16.0.0, new-PM-specific)

1. **C++ standard** (`CMakeLists.txt`): `set(CMAKE_CXX_STANDARD 17)` +
   `CMAKE_CXX_STANDARD_REQUIRED ON`. 16.0.0 is the first release built with
   C++17 by default and its headers use C++17 (`std::is_integral_v` in
   `llvm/ADT/bit.h`); the 16.0.0 prebuilt's CMake config exports no
   `LLVM_OPTIMIZED_CXX_FLAGS` and no standard, so without this the project
   compiled with the host gcc 7.5 `gnu++14` default and failed. (The 15.0.0
   headers happened to be C++14-compatible, so this was latent in the 15
   port.)
2. **`None` removed** (`lib/Giri/TracingNoGiri.cpp`): `getOrInsertF` default
   arg `ArrayRef<Type *> Args = None` → `= {}`. The 3.4 `None` sentinel was
   removed in modern LLVM; the sole default-using call site (`giriCtor`, a
   zero-arg function) makes `{}` behavior-identical.
3. **GLIBCXX shim gate** (`tools/Tracer/CMakeLists.txt`): the 15.0.0 port
   hard-wired `llvm_std_shim.cpp` (it back-filled
   `std::__throw_bad_array_new_length` for the 15.0.0 **rhel-8.4** prebuilt
   whose static libs referenced a libstdc++ symbol the 20.04 host lacked).
   For 16.0.0 the **ubuntu-18.04** prebuilt's static libs reference that symbol
   **0×** (verified with `nm -C` on every `llvm-config --libfiles all` lib),
   and 18.04's libstdc++ lacks the `_GLIBCXX_NODISCARD` macro the shim TU
   uses, so compiling it on would fail. The CMake now **probes the static libs
   at configure time** and compiles the shim TU only when the symbol is
   actually referenced — correct on 15.0.0 (referenced → compiled) and
   16.0.0 (not referenced → skipped) alike.

4. **Opaque pointers (the expected substantive delta) — resolved, no change
   needed.** The task-note spike (2026-08-31 13-07) recorded the flag as
   "accepted but inert, IR comes out `ptr`". That was **wrong on this exact
   prebuilt**. Measured on the 16.0.0 ubuntu-18.04 clang: the harness's
   `-Xclang -no-opaque-pointers` is the **passthrough** form (forwarded to
   cc1) and is **honored** — with the flag the IR is *typed* (`i32*`,
   `i32**`); without it clang 16 emits *opaque* `ptr`. The flag is therefore
   **load-bearing** (removing it re-introduces the kmeans golden drift the
   15.0.0 port measured) and **stays** in `test/Makefile.common` unchanged.
   `git diff 63b02e2..HEAD -- test/` is empty — the harness, goldens, and
   criterion files are all untouched.

## Invariants (re-verified for 16.0.0)

1. **Numbering determinism** — identical pass sequence in both pipeline
   stages (`function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum`,
   `test/Makefile.common` lines 71/116), so the same `bbnum`/`lsnum` IDs are
   assigned in both runs; behaviorally proven by the 22/22 PASS against the
   3.4 goldens.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` unchanged vs the base
   (`git diff 63b02e2..HEAD -- include/Giri/Runtime.h` is empty); re-derived
   in-container: `sizeof(Entry) == 32` on x86_64 LP64, `4096 % 32 == 0`; every
   fresh `.trace` file `% 32 == 0`; `prtrace` decodes all 22 traces.
3. **Debug info** — `-g` mandatory in the test compile; `clang -g` on 16.0.0
   emits `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era
   `file:line` slices across all 22 passing tests.

## Known residuals (inherited; no [regression] rows)

- `opt` needs `-load` + `-load-pass-plugin` per plugin — inherent to the
  14.0.0+ new-PM driver; the harness loads each library twice and documents
  this.
- Pthread variants not re-measured on 16.0.0 — [inherited] gap (the Dockerfile
  pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0
  audit's).
- test6 (sigusr1), test7 (sigint), test22 (fp) — [inherited] gap (have goldens
  but are not in `auto-tests.txt`; signal tests need an interactive terminal,
  test22 needs `-lm`).
- HelloWorld, histogram, linear_regression, word_count — [inherited] gap (no
  golden on any LLVM version; HelloWorld's Makefile runs by hand).
- `ensurePostDomFrontierComputed` per-function `PostDominatorTree`/frontier is
  not freed — [inherited] benign, same `opt`-process-lifetime bound as the
  15.0.0 port, no observable cost.
- `signal(SIGKILL, …)` is a no-op in `runtime/Giri/Tracing.cpp` — [inherited]
  harmless (SIGKILL cannot be caught per POSIX).
