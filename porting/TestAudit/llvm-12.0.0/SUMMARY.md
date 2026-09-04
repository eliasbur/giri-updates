# LLVM 12.0.0 Test Audit Summary

## Baseline suite run

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq` per the
`Dockerfile` env, honest harness with per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED`
and `[GIRI] Abnormal termination` crash detection) on the LLVM 12.0.0 port:

| Result | Count | Tests |
|--------|-------|-------|
| PASS | 22 | test1–5, test8–21 (19 UnitTests), matrix_multiply-seq, pca-seq, kmeans-seq |
| FAIL | 0 | — |

**22 PASS / 0 FAIL** (rc=0), the second clean 22/22 on the legacy-PM line
(after 14.0.0-legacypm). The 5.0.2 standing failure (matrix_multiply-seq
criterion drift) stayed resolved: the `matrix_mult 291` criterion (the
lineage's single criterion-file change, 5.0.2 commit `ec0e6b7`) reproduces the
19-line 3.4 golden exactly under 12.0.0 codegen — no new retune is needed, and
per the golden-file constraint no `test/` file was changed.

## Golden-file provenance (hard constraint)

No `ans-*.txt` golden was changed by this port. Verified:
`git diff 224bdfb..HEAD -- 'test/**/ans-*.txt'` is empty, where `224bdfb` is the
completed 8.0.0 legacy-PM port head this port cuts from; `git diff
224bdfb..HEAD -- test/` is empty in its entirety (the *strongest* clause — the
8.0.0 harness works on 12.0.0 byte-for-byte, zero harness/golden/criterion
changes). The **only** test-file change across the whole port lineage is
`test/matrix_multiply/criterion-inst-seq.txt` (`matrix_mult 285` → `matrix_mult
291`), made on the 5.0.2 branch (commit `ec0e6b7`, "Adapt matrix_mulit inst
criterion to new trace") and inherited — a *criterion* file, not a golden.
Every PASS in this audit is therefore against the pristine 3.4 goldens
(merge-base `86f3b8a` with `origin/master`).

## Per-test verdict table (automated suite, seq variant)

| Test | Verdict | Criterion | Golden | Report |
|------|---------|-----------|--------|--------|
| test1 (extlibcalls) | CLEAN | none | ans-inst.txt (4) | [UnitTests-test1.md](UnitTests-test1.md) |
| test2 (ifelse) | CLEAN | none | ans-inst.txt (4) | [UnitTests-test2.md](UnitTests-test2.md) |
| test3 (fibonacci) | CLEAN | none | ans-inst.txt (9) | [UnitTests-test3.md](UnitTests-test3.md) |
| test4 (example) | CLEAN | criterion-loc.txt (`example.c 26`, `example.c 27`) | ans-inst.txt (7) | [UnitTests-test4.md](UnitTests-test4.md) |
| test5 (hellothreads) | CLEAN | none | ans-inst.txt (12) | [UnitTests-test5.md](UnitTests-test5.md) |
| test8 (ptr) | CLEAN | none | ans-inst.txt (4) | [UnitTests-test8.md](UnitTests-test8.md) |
| test9 (forloop) | CLEAN | none (EXIT_UNCHECKED) | ans-inst.txt (5) | [UnitTests-test9.md](UnitTests-test9.md) |
| test10 (str) | CLEAN | none | ans-inst.txt (4) | [UnitTests-test10.md](UnitTests-test10.md) |
| test11 (hello2p) | CLEAN | none | ans-inst.txt (7) | [UnitTests-test11.md](UnitTests-test11.md) |
| test12 (psum) | CLEAN | none | ans-inst.txt (20) | [UnitTests-test12.md](UnitTests-test12.md) |
| test13 (struct) | CLEAN | none | ans-inst.txt (6) | [UnitTests-test13.md](UnitTests-test13.md) |
| test14 (struct-ptr) | CLEAN | none | ans-inst.txt (7) | [UnitTests-test14.md](UnitTests-test14.md) |
| test15 (hanoi) | CLEAN | none | ans-inst.txt (12) | [UnitTests-test15.md](UnitTests-test15.md) |
| test16 (struct-ptr) | CLEAN | none | ans-inst.txt (7) | [UnitTests-test16.md](UnitTests-test16.md) |
| test17 (plower) | CLEAN | criterion-loc.txt (`plower.c 27`) | ans-inst.txt (6) | [UnitTests-test17.md](UnitTests-test17.md) |
| test18 (extlibcalls) | CLEAN | none | ans-inst.txt (4) | [UnitTests-test18.md](UnitTests-test18.md) |
| test19 (fibocci) | CLEAN | none | ans-inst.txt (10) | [UnitTests-test19.md](UnitTests-test19.md) |
| test20 (even) | CLEAN | none | ans-inst.txt (11) | [UnitTests-test20.md](UnitTests-test20.md) |
| test21 (hwtype) | CLEAN | criterion-inst.txt (`main 51`) | ans-inst.txt (6) | [UnitTests-test21.md](UnitTests-test21.md) |
| matrix_multiply | CLEAN | criterion-inst-seq.txt (`matrix_mult 291`) | ans-inst-seq.txt (19) | [matrix_multiply-seq.md](matrix_multiply-seq.md) |
| pca | CLEAN | criterion-inst-seq.txt (`calc_mean 51`) | ans-inst-seq.txt (17) | [pca-seq.md](pca-seq.md) |
| kmeans | CLEAN | criterion-inst-seq.txt (`main 120`) | ans-inst-seq.txt (2) | [kmeans-seq.md](kmeans-seq.md) |

## Cold-build acceptance (s7)

`/giri/build` wiped **and** test artifacts wiped (`make -C /giri/test clean`),
then `source /giri/utils/build.sh` from `/giri` (cold CMake configure,
`make -j$(nproc)`, full suite): rc=0, **22 PASS / 0 FAIL**, all 5 artifacts
present (`build/lib/{libgiri.so, libdgutility.so, librtgiri.a}`,
`build/bin/{tracer, prtrace}`). Per-test `_test_logs/*.log` show genuine
`STAGE: BUILD` + `STAGE: TEST` re-runs (fresh timestamps). Negative control:
an empty `slice.loc` does **not** match the test1 golden (diff rc≠0) — the
PASS verdicts are non-vacuous. Log: `_cold_acceptance/s7_cold_acceptance.log`.

## Standalone `tracer`/`prtrace` validation (s6, honest scope)

**Root-caused finding (pre-existing legacy-PM-line limitation, not a 12.0.0
regression):** the legacy-PM `tools/Tracer/Tracer.cpp` (the original 3.4 tool;
`git diff 224bdfb..HEAD -- tools/Tracer/Tracer.cpp` is empty) has a *fixed*
pipeline in `main()` — `if (DoTrace) PM.add(TracingNoGiri); else if
(DynamicGiri) PM.add(DynamicGiri);` — and never applies `-bbnum -lsnum` (the
`opt` harness does). `DynamicGiri` → `TraceFile::findPreviousID` reads the
numbering maps via `QueryBasicBlockNumbers`/`QueryLoadStoreNumbers`, so the
standalone tracer's **slice** mode SIGBUSes on invalid numbering. The
8.0.0→14.0.0-legacypm delta (`224bdfb..fba2565`) shows the next legacy port
made no `Tracer.cpp` pipeline change either (only the 14.0.0-only
`F_None`→`OF_None` rename and the `llvm-config --libfiles` link fix), and the
legacy line has never validated standalone-tracer slicing — the slice *result*
is validated via `opt` in the suite above.

**Validated standalone-able scope: 22 PASS / 0 FAIL** over the same 22 cases —
standalone `tracer -trace` instrument → `llc` → link `-lrtgiri` → run the real
program (EXPECTED_EXIT satisfied, 0 Abnormal-termination) → trace non-empty and
`%32==0` → `prtrace` decodes it (`End` record ⇒ `Entry` ABI intact). Cross-check
on test1: the standalone-`tracer` trace is **identical** (prtrace ID/Type
sequence diff empty) to the `opt` harness's `-trace-giri` trace. Evidence:
`_tool_validation/full_tool_validation_12.py`, `_tool_validation/
s6_tool_validation.log`, `_tool_validation/s6_test1_crosscheck.log`.

## Re-verification of the three critical invariants (AGENTS.md)

1. **Numbering determinism** — verified. The instrumentation and slicing stages
   both apply the identical pass sequence (`-mergereturn -bbnum -lsnum …
   -remove-bbnum -remove-lsnum`) to the same `$(NAME).all.bc` (Makefile.common
   lines 45 and 85). Re-derived in-container in the cold-build state: two
   `-bbnum -query-bbnum` runs on the same `.bc` are byte-identical.
   Behaviorally proven by the 22/22 PASS against the 3.4 goldens.
2. **`Entry` struct ABI** — verified. `include/Giri/Runtime.h` is untouched by
   this port (`git diff 224bdfb..HEAD -- include/Giri/Runtime.h` is empty).
   Re-derived in-container: `sizeof(Entry)`=32 on x86_64, `4096 % 32 == 0`;
   fresh test1 trace = 1216 B (38×32, `%32==0`); `prtrace` decodes all 22
   standalone traces ending in the `End` record.
3. **Debug info** — verified. The harness compiles every test with `-g`
   (Makefile.common:23), `SourceLineMapping` reads the DWARF DI metadata, and
   the resulting `file:line` slices match the 3.4-era goldens across all 22
   passing tests.

## Messages in passing tests

- **`opt -stats` prints nothing** — the 12.0.0 prebuilt `opt` (Release build
  without stats) emits no stderr line for `-stats` (0× across the suite logs);
  benign, stderr-only, never pollutes the `.ll`/trace/slice files. (8.0.0/
  14.0.0 printed `Statistics are disabled…`; the 16.0.0 prebuilt is silent —
  same class.)
- **`Start slicing Function:Instruction is defined as …` / `Start slicing
  File:Line …`** — routine per-criterion progress from `Giri.cpp` (test4,
  test17, test21, and the three benchmarks).
- **Program stdout** (`fibonacci(15) is 610`, `MingliangLIU`, matrix prints) —
  expected output of the traced binaries.

## Suite results across the port (legacy-PM line)

| LLVM | Suite result (seq, honest harness) | Standing failures |
|------|------------------------------------|-------------------|
| 3.4 (master) | the 3.4 reference run (pre-port) | the pre-port baseline |
| 5.0.2 | 13 PASS / 9 FAIL, then post-`3b26ea6` re-verified: 1 FAIL (audit `porting/TestAudit/llvm-5.0.2/`) | matrix_multiply-seq (FAIL-EXPECTED drift at `:292`, retuned to `:291` in `ec0e6b7`) |
| 8.0.0 | 22-case suite, 0 FAIL (audit `porting/TestAudit/llvm-8.0.0/`; its summary line's "21" is an undercount of the 22-case `auto-tests.txt`) | none in the automated suite |
| **12.0.0** | **22 PASS / 0 FAIL (this audit)** | none in the automated suite |
| 14.0.0-legacypm | 22 PASS / 0 FAIL (audit `porting/TestAudit/llvm-14.0.0-legacypm/`) | none in the automated suite |

The 5.0.2 standing failure (matrix_multiply-seq) **stayed resolved** on both
8.0.0 and 12.0.0: the `:291` retuned criterion reproduces the 19-line golden
exactly under 12.0.0 codegen. No new retune was required.

## Non-suite test directories (excluded, same as the 8.0.0 audit)

Same set as `porting/TestAudit/llvm-8.0.0/SUMMARY.md` § "Non-suite test
directories": test6 (sigusr1), test7 (sigint), test22 (fp/`-lm`), HelloWorld,
histogram, linear_regression, word_count — none wired into `auto-tests.txt`,
none affected by this port. The pthread variants are likewise out of the
automated suite (`TEST_PARALLELISM=seq`); the 8.0.0 audit's pthread findings
(matrix_multiply-pthread FAIL-EXPECTED criterion drift, kmeans-pthread
FAIL-HARNESS on the 256-CPU host) stand as the last pthread measurements.
