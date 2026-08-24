# LLVM 8.0.0 Test Audit Summary

## Baseline suite run

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq` per the
`Dockerfile` env, honest harness with per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED`
and `[GIRI] Abnormal termination` crash detection) on the LLVM 8.0.0 port:

| Result | Count | Tests |
|--------|-------|-------|
| PASS | 21 | test1–5, test8–21 (18 UnitTests), matrix_multiply-seq, pca-seq, kmeans-seq |
| FAIL | 0 | — |

This is a **strict improvement over the 5.0.2 baseline** (21 PASS / 1 FAIL, the
standalone failure being `matrix_multiply-seq` FAIL-EXPECTED criterion drift at
5.0.2's `matrix_mult:292`). On 8.0.0 the same retuned criterion (`:291`, commit
`ec0e6b7`) reproduces the 19-line golden **exactly** — the 5.0.2 drift did not
recur, so no new retune is needed for the seq variant.

## Golden-file provenance (hard constraint)

No `ans-*.txt` golden was changed by this port. Verified:
`git diff 86f3b8a..HEAD -- 'test/**/ans-*.txt'` is empty, where `86f3b8a` is the
merge-base with `origin/master` (the 3.4 codebase). The **only** test-file change
across the whole port lineage is
`test/matrix_multiply/criterion-inst-seq.txt` (`matrix_mult 285` → `matrix_mult
291`), made on the 5.0.2 branch (commit `ec0e6b7`, "Adapt matrix_mulit inst
criterion to new trace") — a *criterion* file, not a golden. Every PASS in this
audit is therefore against the pristine 3.4 goldens.

## Per-test verdict table (automated suite, seq variant)

| Test | Verdict | Criterion | Golden | Report |
|------|---------|-----------|--------|--------|
| test1 (extlibcalls) | CLEAN | none | ans-inst.txt | [UnitTests-test1.md](UnitTests-test1.md) |
| test2 (ifelse) | CLEAN | none | ans-inst.txt | [UnitTests-test2.md](UnitTests-test2.md) |
| test3 (fibonacci) | CLEAN | none | ans-inst.txt | [UnitTests-test3.md](UnitTests-test3.md) |
| test4 (example) | CLEAN | criterion-inst/loc.txt | ans-inst/loc.txt | [UnitTests-test4.md](UnitTests-test4.md) |
| test5 (hellothreads) | CLEAN | none | ans-inst.txt | [UnitTests-test5.md](UnitTests-test5.md) |
| test8 (ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test8.md](UnitTests-test8.md) |
| test9 (forloop) | CLEAN | none (EXIT_UNCHECKED) | ans-inst.txt | [UnitTests-test9.md](UnitTests-test9.md) |
| test10 (str) | CLEAN | none | ans-inst.txt | [UnitTests-test10.md](UnitTests-test10.md) |
| test11 (hello2p) | CLEAN | none | ans-inst.txt | [UnitTests-test11.md](UnitTests-test11.md) |
| test12 (psum) | CLEAN | none | ans-inst.txt | [UnitTests-test12.md](UnitTests-test12.md) |
| test13 (struct) | CLEAN | none | ans-inst.txt | [UnitTests-test13.md](UnitTests-test13.md) |
| test14 (struct-ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test14.md](UnitTests-test14.md) |
| test15 (hanoi) | CLEAN | none | ans-inst.txt | [UnitTests-test15.md](UnitTests-test15.md) |
| test16 (struct-ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test16.md](UnitTests-test16.md) |
| test17 (plower) | CLEAN | criterion-inst/loc.txt | ans-inst/loc.txt | [UnitTests-test17.md](UnitTests-test17.md) |
| test18 (extlibcalls) | CLEAN | none | ans-inst.txt | [UnitTests-test18.md](UnitTests-test18.md) |
| test19 (fibocci) | CLEAN | none | ans-inst.txt | [UnitTests-test19.md](UnitTests-test19.md) |
| test20 (even) | CLEAN | none | ans-inst.txt | [UnitTests-test20.md](UnitTests-test20.md) |
| test21 (hwtype) | CLEAN | criterion-inst.txt | ans-inst.txt | [UnitTests-test21.md](UnitTests-test21.md) |
| matrix_multiply | CLEAN | criterion-inst-seq.txt (`matrix_mult 291`) | ans-inst-seq.txt (19 lines) | [matrix_multiply-seq.md](matrix_multiply-seq.md) |
| pca | CLEAN | criterion-inst-seq.txt | ans-inst-seq.txt (17 lines) | [pca-seq.md](pca-seq.md) |
| kmeans | CLEAN | criterion-inst-seq.txt | ans-inst-seq.txt (2 lines) | [kmeans-seq.md](kmeans-seq.md) |

## Manual pthread-variant runs (mirroring the 5.0.2 audit)

| Test | Verdict | Diff vs golden | Root cause | Report |
|------|---------|----------------|------------|--------|
| matrix_multiply-pthread | **FAIL-EXPECTED** | 10 of 60 lines missing; 0 extra (monotonic subset) | Criterion instruction drift: 3.4's `matrixmult_map:138` ≠ 8.0.0's #138 (`matrixmult_map` has 148 instructions under 8.0.0 codegen). Sweep 120–148: N=136 closest (58/60, missing only lines 102/103); N=138 gives 50/60. Same drift class as the 5.0.2 seq finding. Out of the automated suite. | [matrix_multiply-pthread.md](matrix_multiply-pthread.md) |
| pca-pthread | CLEAN | empty (34 lines) | none; the 5.0.2 root-cause-A fix (`3b26ea6`) is inherited | [pca-pthread.md](pca-pthread.md) |
| kmeans-pthread | **FAIL-HARNESS** | not reached | `kmeans-pthread.c:316` assert `num_threads == num_procs` fires on the 256-CPU host (100 points < 256 CPUs → 100 threads); SIGABRT caught by the harness's `[GIRI] Abnormal termination` marker; runaway pre-crash iteration wrote a 101 GB trace and slicing timed out. Identical to the 5.0.2 finding (108 GB trace then). | [kmeans-pthread.md](kmeans-pthread.md) |

## The one open item: matrix_multiply-pthread criterion drift

This is the **only** non-CLEAN result of the whole audit, and it is not in the
automated suite. It is a criterion-file (instruction index) drift, not a golden
mismatch and not a slicing bug (the slice is a monotonic subset of the golden with
no extra/wrong lines). Per the golden-file constraint, no test file was changed.
Two resolution options are documented in
[matrix_multiply-pthread.md](matrix_multiply-pthread.md) and deferred to a user
decision:

1. Retune `test/matrix_multiply/criterion-inst-pthread.txt` to the 8.0.0-codegen
   equivalent (the sweep points at `matrixmult_map:136`, with a residual 2-line
   diff on the `out->length` tail-assignment branch; a finer search may find an
   exact reproduction).
2. Leave the criterion at `:138` and carry the FAIL-EXPECTED pthread result as
   documented (consistent with how the 5.0.2 port first carried the seq
   FAIL-EXPECTED before retuning it).

## Re-verification of the three critical invariants (AGENTS.md)

1. **Numbering determinism** — verified. The instrumentation and slicing stages both
   apply the identical pass sequence (`-mergereturn -bbnum -lsnum ... -remove-bbnum
   -remove-lsnum`) to the same `$(NAME).all.bc` (Makefile.common lines 45 and 85), so
   the same `-bbnum`/`-lsnum` IDs are assigned in both runs. Behaviorally proven by
   the 21/21 PASS against the 3.4 goldens: any numbering mismatch would produce
   wrong or empty slices and the goldens would not match.
2. **`Entry` struct ABI** — verified. `include/Giri/Runtime.h` is byte-identical to the
   3.4 base (`git diff 86f3b8a..HEAD -- include/Giri/Runtime.h` is empty), so both the
   layout and the size-divides-page-size invariant are trivially preserved.
3. **Debug info** — verified. The harness compiles every test with `-g`
   (`CFLAGS += -g -O0 -c -emit-llvm`, Makefile.common:23), `SourceLineMapping` reads
   the DWARF DI metadata, and the resulting `file:line` slices match the 3.4-era
   goldens (which are source-line based) across all 21 passing tests.

## Messages in passing tests

- **`Statistics are disabled. Build with asserts or with -DLLVM_ENABLE_STATS`** —
  emitted by `opt` at each `-stats` stage because the prebuilt LLVM 8.0.0 toolchain
  is a Release build without stats. Benign; the 5.0.2 audit noted the analogous
  `-stats` suppression. Same for 8.0.0.
- **`Start slicing Function:Instruction is defined as <fn>:<n>` / `Start slicing
  File:Line ...`** — routine per-criterion progress logging from `Giri.cpp`, once
  per criterion. Not a finding.
- **Program stdout** (e.g. `fibonacci(15) is 610`, matrix prints) — expected output
  of the traced binaries.

## Suite results across the port

| LLVM | Suite result (seq, honest harness) | Standing failures |
|------|------------------------------------|-------------------|
| 3.4 (master) | the 3.4 reference run (pre-port) | the pre-port baseline |
| 5.0.2 | 21 PASS / 1 FAIL (commit `4cd2451`, audit at `porting/TestAudit/llvm-5.0.2/`) | matrix_multiply-seq (FAIL-EXPECTED drift at `:292`) |
| **8.0.0** | **21 PASS / 0 FAIL (this audit)** | none in the automated suite |

The 5.0.2 standing failure (matrix_multiply-seq) **resolved itself** on 8.0.0: the
criterion retuned to `:291` for 5.0.2 happens to reproduce the 19-line golden exactly
under 8.0.0 codegen (verified by a fresh clean build + manual slice, 19/19 IDENTICAL).
No new retune was required.

## Non-suite test directories (excluded, same as the 5.0.2 audit)

Same set as `porting/TestAudit/llvm-5.0.2/SUMMARY.md` § "Non-suite test directories":
test6/test7 (signals), test22 (fp/`-lm`), HelloWorld, histogram, linear_regression,
word_count — none wired into `auto-tests.txt`, none affected by this port.
