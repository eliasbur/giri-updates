# Giri LLVM 15.0.0 test audit (new pass manager)

**Scope.** Functional parity of the Giri 15.0.0 port against the pristine
LLVM 3.4 goldens, run under the **new** pass manager. Suite: the 22 tests in
`test/auto-tests.txt` — 19 UnitTests (test1–5, test8–21) plus the three
benchmark seq variants `matrix_multiply`, `pca`, `kmeans`.

**Bottom line.** The automated suite is **22 PASS / 0 FAIL** (rc=0), matching
the 14.0.0 new-PM port exactly (`suite_final_table.txt` records one `[PASS]`
row per test, 22 rows). A second, independent validation of the
**standalone `tracer`** binary (not `opt`) over the same 22 test cases is also
**22 PASS / 0 FAIL** — the DataLayout self-assignment abort that the 15.0.0
verifier would otherwise trigger is gone (see "Root-cause fixes", below). The
three invariants hold: numbering determinism, `Entry` ABI
(`sizeof(Entry)=32`, `4096 % 32 == 0`), and `-g` debug-info slicing.

Evidence lives alongside this file: the suite's raw per-test stage logs under
`_test_logs/*.log` (per-test build+run+slice logs, `lib-stage.log`,
`suite_final.log`, and the tally in `suite_final_table.txt`), and the
standalone-tracer validation under `_tool_validation/`
(`full_tool_validation.py`, `full-tool-validation.txt`). The 22 per-test
reports (`UnitTests-testN.md` / `<bench>-seq.md`) each carry the golden name
and line count, the input, the criterion actually used, the diff status, and
the captured stage-by-stage output.

## Suite results (22 PASS / 0 FAIL)

All 22 rows `PASS` in `suite_final_table.txt`. Every diff is empty: the
harness's `diff $(NAME).slice.loc $(TEST_ANS)` exits 0, i.e. the 15.0.0
`file:line` slice is byte-identical to the pristine 3.4-era golden.
Golden/criterion values below are the source-tree ground truth
(`ans-*.txt` + `criterion-*.txt` under each test directory); "implicit" means
the test's Makefile passes no explicit `-criterion-*` flag and the slice is
taken at the harness's implicit criterion.

| Test | Golden (lines) | Criterion | Diff vs 3.4 golden |
|------|----------------|-----------|--------------------|
| test1  | ans-inst.txt (4)   | implicit | empty |
| test2  | ans-inst.txt (4)   | implicit | empty |
| test3  | ans-inst.txt (9)   | implicit | empty |
| test4  | ans-inst.txt (7)   | `criterion-loc` | empty |
| test5  | ans-inst.txt (12)  | implicit | empty |
| test8  | ans-inst.txt (4)   | implicit | empty |
| test9  | ans-inst.txt (5)   | implicit | empty |
| test10 | ans-inst.txt (4)   | implicit | empty |
| test11 | ans-inst.txt (7)   | implicit | empty |
| test12 | ans-inst.txt (20)  | implicit | empty |
| test13 | ans-inst.txt (6)   | implicit | empty |
| test14 | ans-inst.txt (7)   | implicit | empty |
| test15 | ans-inst.txt (12)  | implicit | empty |
| test16 | ans-inst.txt (7)   | implicit | empty |
| test17 | ans-inst.txt (6)   | `criterion-loc` | empty |
| test18 | ans-inst.txt (4)   | implicit | empty |
| test19 | ans-inst.txt (10)  | implicit | empty |
| test20 | ans-inst.txt (11)  | implicit | empty |
| test21 | ans-inst.txt (6)   | `criterion-inst` | empty |
| matrix_multiply-seq | ans-inst-seq.txt (19) | `criterion-inst-seq` (`matrix_mult 291`) | empty |
| pca-seq           | ans-inst-seq.txt (17) | `criterion-inst-seq` (`calc_mean 51`)  | empty |
| kmeans-seq        | ans-inst-seq.txt (2)  | `criterion-inst-seq` (`main 120`)      | empty |

## Per-test one-line notes

| Test | One-line note |
|------|---------------|
| test1  | diff empty; 4 slice lines identical to the 3.4 golden. |
| test2  | diff empty; 4/4 identical. |
| test3  | diff empty; 9/9 identical. |
| test4  | diff empty; 7/7 identical; sliced at the explicit `criterion-loc`. |
| test5  | diff empty; 12/12 identical — pthread test (input `8` = thread count). |
| test8  | diff empty; 4/4 identical. |
| test9  | diff empty; 5/5 identical. |
| test10 | diff empty; 4/4 identical. |
| test11 | diff empty; 7/7 identical. |
| test12 | diff empty; 20/20 identical — the longest UnitTests slice. |
| test13 | diff empty; 6/6 identical. |
| test14 | diff empty; 7/7 identical. |
| test15 | diff empty; 12/12 identical. |
| test16 | diff empty; 7/7 identical. |
| test17 | diff empty; 6/6 identical; sliced at the explicit `criterion-loc`. |
| test18 | diff empty; 4/4 identical. |
| test19 | diff empty; 10/10 identical. |
| test20 | diff empty; 11/11 identical; the only test with a Clang-15 stage-2 warning (implicit function declaration, `even.c`), downgraded to a warning by the harness. |
| test21 | diff empty; 6/6 identical; sliced at the explicit `criterion-inst`. |
| matrix_multiply-seq | 19/19 identical; criterion `matrix_mult 291`. |
| pca-seq           | 17/17 identical; criterion `calc_mean 51`. |
| kmeans-seq        | 2/2 identical; criterion `main 120`. |

## Standalone-tracer cross-check (22 PASS / 0 FAIL)

`_tool_validation/` re-runs the 22 test cases driving the `tracer` binary
(which builds its pipeline programmatically with the new PM and a hand-built
`ModuleAnalysisManager`) instead of `opt`. Result: **22 PASS / 0 FAIL**
(`full-tool-validation.txt` records `standalone-tracer FULL RESULT: 22 PASS /
0 FAIL`). This is the path that previously aborted in the 15.0.0 container
with a `DataLayout::ParamMaxAlignment too small` verifier error; it now passes
cleanly because the tracer no longer self-assigns its `DataLayout`.

## Root-cause fixes specific to 15.0.0 (new-PM port)

The 15.0.0 branch inherits the 14.0.0 new-PM port's change set (9.0.0–14.0.0
API fixes, toolchain, Dockerfile) unchanged. The 15.0.0 work adds:

1. **Tracer `DataLayout` self-assignment removed** (`tools/Tracer/Tracer.cpp`).
   The tracer's `run` set `M->setDataLayout(M->getDataLayout())` — a
   **self-assignment** through the hand-written `DataLayout::operator=`
   (`llvm/IR/DataLayout.h:213`, byte-identical in 14.0.0 and 15.0.0). That
   `operator=` calls `clear()` and then copies members from its (same-object)
   source, so the copy reads the just-cleared members and corrupts the layout.
   In ≤14.0.0 `opt` never reached the 15.0.0 verifier check, so the corruption
   was latent. The 15.0.0 module verifier added a
   `DataLayout::ParamMaxAlignment = 1 << 14` check that **aborts the standalone
   tracer** on the corrupted alignment (`DataLayout::ParamMaxAlignment too
   small: 0 … while Verifying Function 'main'`). Removed the self-assignment
   (the no-op it is) — verified root cause in-container.
2. **`tracer` link shim** (`tools/Tracer/llvm_std_shim.cpp`). The prebuilt
   rhel-8.4 LLVM 15.0.0 libs reference
   `std::__throw_bad_array_new_length` (GLIBCXX 3.4.29); focal's libstdc++
   6.0.28 predates that out-of-line helper. A 3-line shim supplies it so the
   tracer links and runs.
3. **Harness parity fixes** (`test/Makefile.common`, `test/HelloWorld/Makefile`):
   - `-Xclang -no-opaque-pointers` — the 15.0.0 clang driver does not expose
     `-opaque-pointers` directly, so opaque pointers are disabled at the Clang
     front end to keep the IR in the explicit-pointer form the port's passes
     and the 3.4-era goldens expect.
   - PIE + implicit function declaration: 15.0.0's clang driver defaults to
     `-fPIE` and treats implicit function declarations as an error. The harness
     handles both so the 3.4-vintage test sources (which rely on implicit
     declarations, e.g. `even.c`) still build.
4. **Toolchain**: ubuntu:20.04 + prebuilt rhel-8.4 LLVM/Clang 15.0.0 at
   `/usr/local/llvm` (`llvm-config --version` = 15.0.0), CMake 3.31.12,
   `find_package(LLVM 15.0 REQUIRED CONFIG)` (Giri-side forward-compat choice;
   the LLVM 15.0.0 CMake floor is 3.5).
5. **Dockerfile**: `ENV DEBIAN_FRONTEND=noninteractive` so apt/tzdata do not
   prompt unattended on ubuntu:20.04.

These are harness/toolchain/verifier-behavior fixes. **No test case, golden, or
criterion file was changed** (`git diff 72258e4..HEAD -- test/` is confined to
the pre-approved harness files; `git diff 72258e4..HEAD -- include/Giri/Runtime.h`
is empty).

## Cross-port results

| Port | Suite (seq) | Notes |
|------|-------------|-------|
| `port/llvm-5.0.2` | 21 PASS / 1 FAIL | post-`3b26ea6`; the 1 FAIL is matrix_multiply-seq, an inherent 3.4→5.0.2 criterion-line drift (FAIL-EXPECTED). Pre-fix baseline was 13 PASS / 9 FAIL; `3b26ea6` fixed the 8 FAIL-BUG tests. |
| `port/llvm-8.0.0` | 22 PASS / 0 FAIL | opt-driven; standalone tracer was a separate validation at the time |
| `port/llvm-14.0.0-legacypm` | 22 PASS / 0 FAIL | legacy PM (`-enable-new-pm=0`); 19 UnitTests + 3 benchmark seq |
| `port/llvm-14.0.0` (new-PM) | 22 PASS / 0 FAIL | the port this branch cuts from (head `72258e4`) |
| `port/llvm-15.0.0` (new-PM) | **22 PASS / 0 FAIL** | **this audit**; plus a 22/22 standalone-tracer validation |

## Invariants verified (15.0.0)

- **Numbering determinism** — identical pass sequence in both pipeline stages
  (`-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum"`),
  so the same `bbnum`/`lsnum` IDs are assigned in the instrumentation and
  slicing runs. Proven behaviorally by 22/22 against the 3.4 goldens.
- **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched
  (`git diff 72258e4..HEAD -- include/Giri/Runtime.h` empty); re-checked in the
  15.0.0 container: `sizeof(Entry)=32`, `4096 % 32 == 0`.
- **Debug info** — `-g` mandatory in the test compile; `clang -g` on 15.0.0
  emits `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era
  `file:line` slices across all 22 tests.

## Known residuals (inherited, not new to 15.0.0)

- **Pthread variants not re-measured on 15.0.0** — `Dockerfile` pins
  `TEST_PARALLELISM=seq`; the suite measures seq only. Same scope as
  5.0.2/8.0.0/14.0.0.
- **test6 (sigusr1), test7 (sigint), test22 (fp)** — have goldens but are not
  in `auto-tests.txt` (signal tests need an interactive terminal; test22 needs
  `-lm`). Inherited from all prior ports.
- **HelloWorld, histogram, linear_regression, word_count** — no golden on any
  LLVM version; not wired into the suite (HelloWorld's Makefile is updated to
  the new-PM harness so it runs by hand).
- **`opt -stats` stderr difference** — the harness passes `-stats` (line 73
  of `test/Makefile.common`), but the 15.0.0 prebuilt `opt` prints nothing to
  stderr for it, whereas the 14.0.0 prebuilt `opt` emits
  `Statistics are disabled.  Build with asserts or with -DLLVM_FORCE_ENABLE_STATS`
  (44 occurrences across the 14.0.0-newpm suite logs; 0 in the 15.0.0 suite
  logs). Harmless: stderr only, never pollutes the `.ll`/trace/IR files, and
  does not change the slice.

## Report index

Per-test / per-benchmark reports (22):

`UnitTests-test1.md` … `UnitTests-test5.md`, `UnitTests-test8.md` …
`UnitTests-test21.md` (19 UnitTests), `matrix_multiply-seq.md`, `pca-seq.md`,
`kmeans-seq.md`.

Raw evidence: `_test_logs/` (suite, per-stage logs + `suite_final_table.txt`),
`_tool_validation/` (standalone tracer).
