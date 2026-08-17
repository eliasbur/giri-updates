# LLVM 5.0.2 Test Audit Summary

## Baseline suite run

| Result | Count | Tests |
|--------|-------|-------|
| PASS | 13 | test1, test2, test4, test13, test14, test15, test18, test19, test20, test21, pca, kmeans |
| FAIL | 9 | test3, test5, test8, test9, test10, test11, test12, test17, matrix_multiply |

Matches the 13 PASS / 9 FAIL split recorded in `llvm-5-port.md` exactly.

## Per-test verdict table

| Test | Variant | Verdict | Commit | Current result | Root cause | Report |
|------|---------|---------|--------|----------------|------------|--------|
| test1 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test1.md](UnitTests-test1.md) |
| test2 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test2.md](UnitTests-test2.md) |
| test3 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return empties frontier map, missing line 17 | [UnitTests-test3.md](UnitTests-test3.md) |
| test4 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty; "Start slicing..." messages are routine per-criterion output | [UnitTests-test4.md](UnitTests-test4.md) |
| test5 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 2 "Could not find Control-dep", 10 lines missing | [UnitTests-test5.md](UnitTests-test5.md) |
| test8 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 2 "Could not find Control-dep", line 12 missing | [UnitTests-test8.md](UnitTests-test8.md) |
| test9 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 28 "Could not find Control-dep", line 9 missing | [UnitTests-test9.md](UnitTests-test9.md) |
| test10 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, "Could not find Control-dep", line 13 missing | [UnitTests-test10.md](UnitTests-test10.md) |
| test11 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 5 "Could not find Control-dep", 4 lines missing | [UnitTests-test11.md](UnitTests-test11.md) |
| test12 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 32 "Could not find Control-dep", 3 lines missing | [UnitTests-test12.md](UnitTests-test12.md) |
| test13 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test13.md](UnitTests-test13.md) |
| test14 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test14.md](UnitTests-test14.md) |
| test15 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test15.md](UnitTests-test15.md) |
| test16 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | TraceFile.cpp:378 findNextNestedID: store ID collides with BB ID, fatal crash | [UnitTests-test16.md](UnitTests-test16.md) |
| test17 | seq (no pthread variant) | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 early return, 5 "Could not find Control-dep", 2 lines missing | [UnitTests-test17.md](UnitTests-test17.md) |
| test18 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test18.md](UnitTests-test18.md) |
| test19 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test19.md](UnitTests-test19.md) |
| test20 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty; clang stage 2 warning about implicit declaration is expected for mutual recursion | [UnitTests-test20.md](UnitTests-test20.md) |
| test21 | seq (no pthread variant) | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [UnitTests-test21.md](UnitTests-test21.md) |
| matrix_multiply | seq | FAIL-EXPECTED | pre-`3b26ea6` | unchanged | Criterion `matrix_mult:285` is 3.4's equivalent of 5.0.2's `matrix_mult:292` (both `dprintf("\n")` at line 97, +7 drift within output-printing loop); 5.0.2's #285 is the value-print `fprintf` at line 94 — 18/19 golden lines preserved (shared data chain), 16 DD-sourced extras, 1 missing (line 97, separate fprintf) | [matrix_multiply-seq.md](matrix_multiply-seq.md) |
| matrix_multiply | pthread | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`; re-verified `llvm-5-matrix-multiply-verdict`) | PostDominatorFrontier.cpp:37 early return, 30 "Could not find Control-dep", 25 lines missing | [matrix_multiply.md](matrix_multiply.md) |
| pca | seq | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [pca-seq.md](pca-seq.md) |
| pca | pthread | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`; re-verified `llvm-5-matrix-multiply-verdict`) | PostDominatorFrontier.cpp:37 early return, 40 "Could not find Control-dep", 8 lines missing | [pca.md](pca.md) |
| kmeans | seq | CLEAN | pre-`3b26ea6` | unchanged | Diff empty, no non-routine output | [kmeans-seq.md](kmeans-seq.md) |
| kmeans | pthread | FAIL-HARNESS | pre-`3b26ea6` | **unchanged** (harness enhanced `e194151` to detect crash via `[GIRI] Abnormal termination`) | 256-CPU container triggers assertion failure, 108 GB trace, slicing times out; criterion `main 402` is an instruction index, not a source line | [kmeans.md](kmeans.md) |

## Distinct root causes

### Root cause A: `PostDominatorFrontier.cpp:37` early return at virtual root (FAIL-BUG) — 10 tests

**Affected tests:** test3, test5, test8, test9, test10, test11, test12, test17, matrix_multiply, pca

**Description:** The port's `calculate()` function (`PostDominatorFrontier.cpp:37`) replaced the original 3.4 code's two-part structure with a single early return:
- **3.4 (master):** `if (getRoots().empty()) return S;` followed by `if (BB) { …pred loop… }` (null guard wraps only predecessor iteration; child recursion always runs)
- **Port:** `if (!BB) return S;` (returns immediately at virtual root, skipping all child recursion)

When a function has multiple exit points (e.g., `exit()` + `return`), LLVM 5.0.2's `PostDominatorTree` creates a virtual root node with `getBlock() == nullptr`. The port's early return exits before recursing into children, leaving the `Frontiers` map empty for the entire function. This causes every `PDF.getFrontier()` call to return an empty set, `getExecForcer()` to return `nullptr`, and the slicing pass to emit "Could not find Control-dep of this Basic Block" warnings (`Giri.cpp:153`) while dropping control-dependent lines from the slice.

**Evidence:** Verified against `git show master:lib/Utility/PostDominatorFrontier.cpp`. All 10 affected tests involve `main()` with 2+ exit paths (exit calls + return). The specific line missing in each case is always a control-dependent branch at the top of `main()` or a control-dependent setup instruction it guards.

### Root cause B: `TraceFile.cpp:378` `findNextNestedID` namespace collision (FAIL-BUG) — 1 test

**Affected tests:** test16

**Description:** The `findNextNestedID` function uses an instruction-level ID as `nestID` parameter, but searches for BB entries in the trace. When a BB ID happens to match an instruction ID (e.g., 4th basic block numbered 4, 4th instrumented store also numbered 4), the search falsely increments nesting and cannot find the target entry, resulting in `report_fatal_error("Did not find desired subsequent entry in trace!")` at `TraceFile.cpp:410`. This is a pre-existing latent bug in the original codebase; LLVM 5's different codegen makes the ID collision manifest in test16's IR layout.

### Root cause C: Harness/environment incompatibility (FAIL-HARNESS) — 1 test

**Affected tests:** kmeans

**Description:** The container has 256 CPUs. kmeans-pthread.c uses `sysconf(_SC_NPROCESSORS_ONLN)` for thread count, but asserts `num_threads == num_procs`. With 100 points and 256 CPUs, `num_per_thread = 0`, only 100 threads are created, and the assertion fires. Additionally, `criterion-inst-pthread.txt` specifies `main 402` — this is the 402nd LLVM instruction in `main` (not a source line number).

## Messages in passing tests

### Routine messages (CLEAN verdict, not a finding)

- **"Start slicing Filename:Loc..."** / **"Start slicing Function:Instruction..."** — emitted from `Giri.cpp:248` / `Giri.cpp:291` via `dbgs()`, once per criterion location. Observed in test4 (2 criteria) and other passing tests using explicit criteria. This is routine progress logging.

- **`-stats` output** — emitted by `opt` at end of each pass. Routine, not a finding.

- **Program stdout** — expected output of the instrumented binary (e.g., `fibonacci(15) is 610`, `5\n1048576\n`). Expected.

### Warning in passing test (not a finding)

- **test20 clang warning:** `even.c:8:16: warning: implicit declaration of function 'is_odd' is invalid in C99` — expected for mutual recursion without forward declaration. Does not affect correctness.

## Reconciliation of audit verdicts and subsequent fixes

### The original "all 9 failures share one root cause" claim: **REFUTED**

The original `llvm-5-port.md` stated:

> All 9 failures share the same pattern: "Could not find Control-dep of this Basic Block" warnings result in incomplete dynamic slices [...] This is a legitimate consequence of the LLVM version upgrade and cannot be "fixed".

This audit refutes that claim on two counts:

1. **Not all failures share one root cause.** Of the 11 FAIL-BUG tests discovered by the audit, 10 trace to root cause A (`PostDominatorFrontier.cpp:37`) and 1 traces to root cause B (`TraceFile.cpp:378`). The original baseline only caught 8 of the root-cause-A tests plus matrix_multiply-seq (which is actually FAIL-EXPECTED, see below).

2. **The failures _can_ be fixed.** Both root causes are defects in the port's code, not inherent consequences of the LLVM version upgrade. All 11 FAIL-BUG tests were fixed by commit `3b26ea6` (`llvm-5-test-fixes`).

### Variant clarification: baseline vs. audit

The baseline sweep ran **seq** variants (`Dockerfile:5` sets `TEST_PARALLELISM=seq`). This resolves two discrepancies apparent in the initial audit: the baseline reported pca and kmeans as PASS because it ran `pca-seq` and `kmeans-seq`, both of which are CLEAN. The audit's pthread-variant reports (`pca.md`, `kmeans.md`) found FAIL-BUG and FAIL-HARNESS respectively. The per-test verdict table records each variant separately and correctly.

### Final tally (audit verdicts, with fix annotations)

| Category | Count | Tests | Status |
|----------|-------|-------|--------|
| CLEAN | 13 | test1, test2, test4, test13–15, test18–21, matrix_multiply-seq (see below), pca-seq, kmeans-seq | All passing |
| FAIL-BUG → CLEAN | 10 | test3, test5, test8–12, test17, matrix_multiply-pthread, pca-pthread | Fixed by `3b26ea6` (root cause A) |
| FAIL-BUG → CLEAN | 1 | test16 | Fixed by `3b26ea6` (root cause B) |
| FAIL-HARNESS | 1 | kmeans-pthread | Unfixed; container CP count triggers assertion; harness now detects crash via `[GIRI] Abnormal termination` marker (commit `e194151`) |
| FAIL-EXPECTED | 1 | matrix_multiply-seq | Criterion drift between LLVM 3.4 and 5.0.2; documented at `df93296`; 16 extra lines are traceable to the drift |

Of the 10 root-cause-A tests, 8 (test3, test5, test8–12, test17) were in the original 9 FAIL plus matrix_multiply-seq's seq variant. The remaining 2 (matrix_multiply-pthread, pca-pthread) were discovered by the audit when examining the pthread variants. Of the 9 original FAIL, 8 share root cause A and 1 (matrix_multiply-seq) is actually FAIL-EXPECTED. Thus the original claim was partially correct (all 8 original PostDominatorFrontier failures do share the same symptom) but wrong about fixability.

## Suite results across the port

Six suite runs have been recorded during the `port/llvm-5.0.2` port. The last row is the current result.

| Result | Commit | Harness |
|--------|--------|---------|
| 13 PASS / 9 FAIL | pre-`3b26ea6` | suppressive |
| 21 PASS / 1 FAIL | `3b26ea6` | suppressive (post code-fixes) |
| 7 PASS / 15 FAIL | `2fb3b6d` | honest, before the exit-status opt-in |
| 21 PASS / 1 FAIL | `5fbca9d` | honest, with `EXPECTED_EXIT` (per-test breakdown at [llvm-5-harness-fallout.md](../../TaskNotes/Tasks/llvm-5-harness-fallout.md)) |
| 21 PASS / 1 FAIL | `e194151` | honest + crash detection |
| 21 PASS / 1 FAIL | `4cd2451` | honest + crash detection, `*.trace.err` recipe — **current** |

The single standing failure in every post-fix run is `matrix_multiply-seq` (`FAIL-EXPECTED`, criterion drift). `kmeans-pthread` crashes on the container's 256-CPU host and is caught by the harness's `[GIRI] Abnormal termination` marker; it is not in the automated suite (`Dockerfile:5` pins `TEST_PARALLELISM=seq`).

## Unresolved questions

1. **pca baseline discrepancy:** ~~The baseline sweep reported pca as PASS, but the audit shows 8 lines missing from the slice. The Makefile would have detected this via `diff`. Possible explanation: the Makefile's `make clean -s -C $$t` between tests may have interacted differently, or the build artifacts from a previous test may have been reused.~~ **RESOLVED (2026-08-13, llvm-5-seq-variant-failures):** `Dockerfile:5` sets `ENV TEST_PARALLELISM=seq`. The test Makefiles use `TEST_PARALLELISM ?= pthread`, which yields to the environment variable. The baseline sweep ran **pca-seq** (not pca-pthread). The pthread audit (`pca.md`) found FAIL-BUG with 8 lines missing, but **pca-seq passes cleanly** (verified in this task, report at `pca-seq.md`). The baseline correctly reported pca as PASS because it was running the seq variant.

2. **kmeans baseline discrepancy:** ~~The baseline sweep reported kmeans as PASS, but the audit shows a crash in stage 7 (assertion failure) and stage 8 timeout. The `-` prefix hides the crash, and with a 108 GB trace file, `make test` may have been running for hours (or the build timed out before reaching it). The baseline `make -C test` run completed in a reasonable time, suggesting the Makefile may have short-circuited or used cached artifacts.~~ **RESOLVED (2026-08-13, llvm-5-seq-variant-failures):** Same variant explanation. The baseline ran **kmeans-seq** (not kmeans-pthread). The pthread audit (`kmeans.md`) found FAIL-HARNESS with 256-CPU assertion failure and 108 GB trace, but **kmeans-seq passes cleanly** (verified in this task, report at `kmeans-seq.md`). kmeans-seq.c uses a single thread, does not assert on thread count, and generates a small trace. The baseline correctly reported kmeans as PASS because it was running the seq variant.

3. **`-stats` output:** The prebuilt LLVM 5.0.2 toolchain includes `-stats` in the slicing pipeline. These emit summary counters (e.g., `Number of Dynamic Values in Slice`) to stderr. The Makefile discards these via `> /dev/null 2>&1`. All passing tests produced `-stats` output; not a finding, but worth noting.

## Non-suite test directories (excluded from audit)

| Directory | Exclusion reason |
|-----------|-----------------|
| UnitTests/test6 (sigusr1) | Has golden file (`ans-inst.txt`) but not listed in `auto-tests.txt`; signals-based test (SIGUSR1) requires interactive terminal setup |
| UnitTests/test7 (sigint) | Has golden file (`ans-inst.txt`) but not listed in `auto-tests.txt`; signals-based test (SIGINT/SIGALRM) |
| UnitTests/test22 (fp) | Has golden file (`ans-inst.txt`) and `criterion-inst.txt` but not listed in `auto-tests.txt`; floating-point test with `-lm` dependency |
| HelloWorld | No golden file; custom Makefile with no `test:` target; prints slice directly to stdout |
| histogram | No golden file; configurable via `TEST_PARALLELISM` (pthread/seq); parallel version lacks golden answers |
| linear_regression | No golden file; configurable via `TEST_PARALLELISM`; parallel version lacks golden answers; custom Makefile (not included via `Makefile.common`) |
| word_count | No golden file; configurable via `TEST_PARALLELISM`; requires input file (`Makefile`); not wired into suite |

## Two PostDominanceFrontier suspects — checked

### Suspect 1: Early return at virtual root (CONFIRMED)

The port's `PostDominatorFrontier.cpp:37` uses `if (!BB) return S;` which returns before child recursion. The original 3.4 code's `if (BB) { … }` guard wraps only the predecessor loop, and the `if (getRoots().empty()) return S;` guard runs before any per-node processing. The port's single early return collapses both guards and critically skips the child recursion for virtual-root nodes. This is a **confirmed bug** in the port.

### Suspect 2: Changed `properlyDominates` overload (CONFIRMED as structural difference, impact unclear)

The 3.4 code used `DT.properlyDominates(Node, DT[*CDFI])` (DomTreeNode overload). The port uses `DT.properlyDominates(Node->getBlock(), *CDFI)` (BasicBlock overload, with `*CDFI` being a `BasicBlock*`). The two overloads should agree for every reachable node, but the port passes `Node->getBlock()` which is `nullptr` at the virtual root. Since suspect 1 causes the virtual root's children to never be reached, this overload difference has not been exercised. If suspect 1 is fixed, this overload change would need separate verification.

### `ensurePostDomFrontierComputed` (verified)

`DynamicGiri::ensurePostDomFrontierComputed` (`Giri.cpp:67`) constructs `PostDominatorTreeWrapperPass` with `new`, calls `runOnFunction` outside any pass manager, caches per `Function*`, and never frees. The tree it produces is correct for the current function state. The cache is populated during the `-dgiri` pass (which runs after `-bbnum`/`-lsnum`), so it is not affected by pipeline ordering. However, the memory leak (never freeing the `new`ed objects) is worth noting for a cleanup task.