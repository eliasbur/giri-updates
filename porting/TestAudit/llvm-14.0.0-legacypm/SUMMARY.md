# LLVM 14.0.0 (legacy pass manager) Test Audit Summary

Port branch: `agent/open-code/llvm-14-legacypm` → target `port/llvm-14.0.0-legacypm`.
Cut from the completed `port/llvm-8.0.0` port head (`224bdfb`). Evidence commit: `74b870f`.
Image: `giri-llvm-14` (ubuntu:18.04, prebuilt LLVM/Clang 14.0.0 GitHub-Releases tarball, x86_64).

## Baseline suite run

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq` per the `Dockerfile` env,
honest harness with per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED` and `[GIRI] Abnormal
termination` crash detection) on the LLVM 14.0.0 **legacy-pass-manager** port. Every `opt`
invocation runs `-enable-new-pm=0` (legacy PM), the 14.0.0 execution path for this branch:

| Result | Count | Tests |
|--------|-------|-------|
| PASS | 22 | test1–5, test8–21 (19 UnitTests), matrix_multiply-seq, pca-seq, kmeans-seq |
| FAIL | 0 | — |

The suite is exactly the 22 entries of `test/auto-tests.txt`: 19 UnitTests (test1–test5,
test8–test21) plus the three app benchmarks in their **seq** variant (the Makefiles yield to
`TEST_PARALLELISM=seq` from the Dockerfile env). **Note on the count:** the 8.0.0 audit's
baseline table recorded "21 PASS / 18 UnitTests"; its own per-test verdict table lists 22
(19 UnitTests + 3 apps), and `test/auto-tests.txt` has 22 entries. The 14.0.0 run
reproduces **22 PASS / 0 FAIL**, matching the per-test table and the actual suite file.
No test case, golden file, or criterion file was changed by this port.

## Golden-file provenance (hard constraint)

No `ans-*.txt` golden and no `criterion-*.txt` criterion was changed by this port. Verified:
`git diff 224bdfb..HEAD -- test/` is **empty except** the pre-approved harness lines in
`test/Makefile.common` and `test/HelloWorld/Makefile` (the `-enable-new-pm=0` legacy-PM flag on
the `opt` invocations; 7 lines total). Every PASS in this audit is therefore against the
pristine 3.4 goldens, exactly as on the 8.0.0 port.

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
| test16 (calc + struct-ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test16.md](UnitTests-test16.md) |
| test17 (plower) | CLEAN | criterion-inst/loc.txt | ans-inst/loc.txt | [UnitTests-test17.md](UnitTests-test17.md) |
| test18 (extlibcalls) | CLEAN | none | ans-inst.txt | [UnitTests-test18.md](UnitTests-test18.md) |
| test19 (fibocci) | CLEAN | none | ans-inst.txt | [UnitTests-test19.md](UnitTests-test19.md) |
| test20 (even) | CLEAN | none | ans-inst.txt | [UnitTests-test20.md](UnitTests-test20.md) |
| test21 (hwtype) | CLEAN | criterion-inst.txt | ans-inst.txt | [UnitTests-test21.md](UnitTests-test21.md) |
| matrix_multiply-seq | CLEAN | criterion-inst-seq.txt (`matrix_mult 291`) | ans-inst-seq.txt (19 lines) | [matrix_multiply-seq.md](matrix_multiply-seq.md) |
| pca-seq | CLEAN | criterion-inst-seq.txt (`calc_mean 51`) | ans-inst-seq.txt (17 lines) | [pca-seq.md](pca-seq.md) |
| kmeans-seq | CLEAN | criterion-inst-seq.txt (`main 120`) | ans-inst-seq.txt (2 lines) | [kmeans-seq.md](kmeans-seq.md) |

The 5.0.2 standing failure (`matrix_multiply-seq`) and its 8.0.0 sibling
(`matrix_multiply-pthread` FAIL-EXPECTED) do **not** recur in the automated suite: the
retuned seq criterion (`matrix_mult 291`) reproduces the 19-line golden exactly under
14.0.0 codegen. The pthread variants are out of the automated suite (same as the 8.0.0 audit).

## Re-verification of the three critical invariants (AGENTS.md)

1. **Numbering determinism** — verified. The instrumentation and slicing stages both apply the
   identical pass sequence (`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum`) to the
   same `$(NAME).all.bc` (`test/Makefile.common` lines 42–48 and 82–88), so the same
   `-bbnum`/`-lsnum` IDs are assigned in both runs. Behaviorally proven by the 22/22 PASS
   against the 3.4 goldens: any numbering mismatch would produce wrong or empty slices and the
   goldens would not match.
2. **`Entry` struct ABI** — verified. `include/Giri/Runtime.h` is untouched by this port
   (`git diff 224bdfb..HEAD -- include/Giri/Runtime.h` is empty), so the layout and the
   size-divides-page-size invariant are preserved. Re-checked in-container on 14.0.0:
   `sizeof(Entry) == 32`, and `4096 % 32 == 0` (page-divisible); the runtime assertion in
   `librtgiri` is exercised by every traced run in the passing suite.
3. **Debug info** — verified. The harness compiles every test with `-g`
   (`CFLAGS += -g -O0 -c -emit-llvm`, `test/Makefile.common:23`); `SourceLineMapping` reads the
   DWARF `DI` metadata. Re-checked in-container: `clang -g` on 14.0.0 emits `.debug_info` /
   `.debug_line` sections, and the resulting `file:line` slices match the 3.4-era goldens (which
   are source-line based) across all 22 passing tests.

## Messages in passing tests

- **Program stdout** (e.g. `fibonacci(15) is 610`, matrix prints, `Final Means:`) — expected
  output of the traced binaries. Captured per test in the `_test_logs` excerpts inside each
  per-test report.
- **`Start slicing Function:Instruction is defined as <fn>:<n>` / `Start slicing
  Filename:Loc is defined as <file>:<line>`** — routine per-criterion progress logging from
  `Giri.cpp`, once per criterion. Not a finding.
- **`even.c:8:16: warning: implicit declaration of function 'is_odd' is invalid in C99`**
  (test20 only) — a benign source-level C warning emitted by clang 14.0.0 during the `-c`
  stage; the program compiles, runs, and its slice matches the golden. Not a finding.
- (No `Statistics are disabled` note appeared in this run's logs; the prebuilt 14.0.0 Release
  toolchain suppresses it the way the 8.0.0 audit described for its toolchain.)

## Non-suite test directories (excluded, same as the 8.0.0 / 5.0.2 audits)

test6/test7 (signals), test22 (fp/`-lm`), HelloWorld, histogram, linear_regression, word_count
— none wired into `test/auto-tests.txt`, none affected by this port.

## Suite results across the port

| LLVM | Suite result (seq, honest harness) | Standing failures |
|------|------------------------------------|-------------------|
| 5.0.2 | 21 PASS / 1 FAIL (audit at `porting/TestAudit/llvm-5.0.2/`) | matrix_multiply-seq (FAIL-EXPECTED drift at `:292`) |
| 8.0.0 | 22 PASS / 0 FAIL (audit at `porting/TestAudit/llvm-8.0.0/`) | none in the automated suite |
| **14.0.0 (legacy PM)** | **22 PASS / 0 FAIL (this audit)** | none in the automated suite |
