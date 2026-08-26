# LLVM 14.0.0 (new pass manager) Test Audit Summary

Port branch: `agent/jcode/llvm-14-newpm-port` → target `port/llvm-14.0.0`.
Cut from `fba2565` (the completed `port/llvm-14.0.0-legacypm` head, `224bdfb`
content + closeout commits). Evidence commits: `cd97e71` (libdgutility),
`ac2e688` (libgiri), `70ffef8` (Tracer), `d9a2212` (harness).
Image: `giri-llvm-14` (ubuntu:18.04, prebuilt LLVM/Clang 14.0.0 GitHub-Releases
tarball, x86_64).

## What this port changes vs the legacy-PM port

Everything except the pass-manager choice is inherited from the 14.0.0
legacy-PM port (PR #19): toolchain, Dockerfile, 8→14 C++ API fixes, honest
harness mechanics. This port rewrites the execution path onto the **new pass
manager** — the forward-compatible path that survives LLVM 15+, where the
legacy PM is gone (the 14.0.0 release notes deprecate it: "will be removed
after LLVM 14"). Concretely:

- All Giri/Utility passes/analyses converted from legacy-PM classes
  (`PassInfoMixin` + `getAnalysisUsage` / `RegisterPass`) to new-PM
  `llvm::Pass` / `llvm::Analysis` classes with `run(...)` methods.
  Pass **logic is byte-identical** (no rewrites of the algorithm bodies;
  only the plumbing around them changed).
- `TracingNoGiri` (the former `BasicBlockPass`, deleted in 10.0.0) is now a
  new-PM **module pass**: `run(Module&, MAM)` does the module-level
  initialization once, then loops functions and, per function, basic blocks,
  driving the renamed `instrument`/`runOnBasicBlock` per-BB logic. The per-BB
  order and instrumentation insertion order are preserved.
- `DynamicGiri` is a new-PM **module analysis** (`AnalysisKey Key`), fetched
  by the `dgiri`/`test-giri` module passes via `MAM.getResult<DynamicGiri>(M)`
  (the new-PM result object carries the state the legacy pass instance carried).
- Both libraries export `llvmGetPassPluginInfo` (`lib/Utility/
  UtilityPassPlugin.cpp`, `lib/Giri/GiriPassPlugin.cpp`); the harness loads
  each library **twice** — `-load` (registers the plugin's `cl::opt` globals
  *before* `opt` parses the command line: `-trace-file`, `-slice-file`,
  `-criterion-*`, `-dump-bbid`, `-mapping-*`) plus
  `-load-pass-plugin` (registers the `-passes` pipeline names *after*
  parsing). This double-load is the 14.0.0-specific replacement for the
  legacy `-load`-only invocation and is what makes the `-passes` pipeline and
  the `cl::opt` flags work in one `opt` invocation.
- The `Tracer` tool builds its instrumentation pipeline programmatically with
  the new PM (`ModuleAnalysisManager` + `PassBuilder`, `WriteBitcodeToFile`
  is void in 14.0.0).

**Numbering determinism (invariant 1)** is preserved by design: the
no-op `bbnum`/`lsnum` pipeline entries force the lazy
`QueryBasicBlockNumbers`/`QueryLoadStoreNumbers` **analyses**, which carry
the ID maps — exactly mirroring the legacy `getAnalysis`-triggered behavior.
The IDs are assigned in both the instrumentation and slicing pipeline stages,
which run the identical pass sequence.

**`mergereturn` parity:** no test-only `MergeReturn` was needed. In 14.0.0
the legacy and new-PM `mergereturn` are the *same* transform
(`UnifyFunctionExitNodes`, `lib/Transforms/Utils/`); spike-verified
**byte-identical** IR output. The harness wraps it in an explicit
`function(mergereturn)` sub-pipeline (it must be first to fix the top-level
element type); every Giri pass is a module pass at the top level.

## Baseline suite run

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq` per the
`Dockerfile` env, honest harness with per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED`
and `[GIRI] Abnormal termination` crash detection) on the LLVM 14.0.0
**new-pass-manager** port. Every `opt` invocation runs
`-passes="function(mergereturn),bbnum,lsnum,…"` (new PM; **no**
`-enable-new-pm=0`):

| Result | Count | Tests |
|--------|-------|-------|
| PASS | 22 | test1–5, test8–21 (19 UnitTests), matrix_multiply-seq, pca-seq, kmeans-seq |
| FAIL | 0 | — |

Suite result captured: 22 `[PASS]` lines, `SUITE_DONE rc=0`. The suite is
exactly the 22 entries of `test/auto-tests.txt`: 19 UnitTests plus the three
app benchmarks in their **seq** variant. No test case, golden file, or
criterion file was changed by this port.

## Golden-file provenance (hard constraint)

No `ans-*.txt` golden and no `criterion-*.txt` criterion was changed by this
port. Verified: `git diff fba2565..HEAD -- test/` is **empty except** the
pre-approved harness files `test/Makefile.common` and `test/HelloWorld/Makefile`
(new-PM `opt` invocations; the `-enable-new-pm=0` + legacy `-<passname>` flags
replaced by `-load` + `-load-pass-plugin` + `-passes="…"`). Every PASS is
therefore against the pristine 3.4 goldens, exactly as on the legacy-PM and
8.0.0 ports.

## Per-test verdict table (automated suite, seq variant)

| Test | Verdict | Criterion | Golden | Report |
|------|---------|-----------|--------|--------|
| test1 (extlibcalls) | CLEAN | none | ans-inst.txt (4 lines) | [UnitTests-test1.md](UnitTests-test1.md) |
| test2 (ifelse) | CLEAN | none | ans-inst.txt (4 lines) | [UnitTests-test2.md](UnitTests-test2.md) |
| test3 (fibonacci) | CLEAN | none | ans-inst.txt (5 lines) | [UnitTests-test3.md](UnitTests-test3.md) |
| test4 (example) | CLEAN | criterion-loc.txt | ans-inst.txt (7 lines; == ans-loc.txt) | [UnitTests-test4.md](UnitTests-test4.md) |
| test5 (hellothreads) | CLEAN | none | ans-inst.txt | [UnitTests-test5.md](UnitTests-test5.md) |
| test8 (ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test8.md](UnitTests-test8.md) |
| test9 (forloop) | CLEAN | none (EXIT_UNCHECKED=1) | ans-inst.txt (5 lines) | [UnitTests-test9.md](UnitTests-test9.md) |
| test10 (str) | CLEAN | none | ans-inst.txt | [UnitTests-test10.md](UnitTests-test10.md) |
| test11 (hello2p) | CLEAN | none | ans-inst.txt | [UnitTests-test11.md](UnitTests-test11.md) |
| test12 (psum) | CLEAN | none | ans-inst.txt | [UnitTests-test12.md](UnitTests-test12.md) |
| test13 (struct) | CLEAN | none | ans-inst.txt | [UnitTests-test13.md](UnitTests-test13.md) |
| test14 (struct-ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test14.md](UnitTests-test14.md) |
| test15 (hanoi) | CLEAN | none | ans-inst.txt | [UnitTests-test15.md](UnitTests-test15.md) |
| test16 (struct-ptr) | CLEAN | none | ans-inst.txt | [UnitTests-test16.md](UnitTests-test16.md) |
| test17 (plower) | CLEAN | criterion-loc.txt | ans-inst.txt (6 lines; == ans-loc.txt) | [UnitTests-test17.md](UnitTests-test17.md) |
| test18 (extlibcalls) | CLEAN | none | ans-inst.txt | [UnitTests-test18.md](UnitTests-test18.md) |
| test19 (fibocci) | CLEAN | none | ans-inst.txt | [UnitTests-test19.md](UnitTests-test19.md) |
| test20 (even) | CLEAN | none | ans-inst.txt | [UnitTests-test20.md](UnitTests-test20.md) |
| test21 (hwtype) | CLEAN | criterion-inst.txt | ans-inst.txt (6 lines) | [UnitTests-test21.md](UnitTests-test21.md) |
| matrix_multiply-seq | CLEAN | criterion-inst-seq.txt (`matrix_mult 291`) | ans-inst-seq.txt (19 lines) | [matrix_multiply-seq.md](matrix_multiply-seq.md) |
| pca-seq | CLEAN | criterion-inst-seq.txt (`calc_mean 51`) | ans-inst-seq.txt (17 lines) | [pca-seq.md](pca-seq.md) |
| kmeans-seq | CLEAN | criterion-inst-seq.txt (`main 120`) | ans-inst-seq.txt (2 lines) | [kmeans-seq.md](kmeans-seq.md) |

(test4/test17 slice with `-criterion-loc` but the harness diffs
`$(NAME).slice.loc` against the default `ans-inst.txt`; in both cases
`ans-loc.txt` and `ans-inst.txt` are byte-identical, so the diff is the same
either way — inherited from the 3.4 test design, not a port artifact.)

## Re-verification of the three critical invariants (AGENTS.md)

1. **Numbering determinism** — verified. The instrumentation and slicing
   stages both apply the identical pass sequence
   (`-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum"`)
   to the same `$(NAME).all.bc` (`test/Makefile.common`, the `$(NAME).slice`
   and `$(NAME).trace.bc` rules), so the same `bbnum`/`lsnum` IDs are assigned
   in both runs. Behaviorally proven by the 22/22 PASS against the 3.4 goldens:
   any numbering mismatch would produce wrong or empty slices and the goldens
   would not match. `mergereturn` parity (byte-identical IR, legacy vs new-PM)
   was spike-verified separately.
2. **`Entry` struct ABI** — verified. `include/Giri/Runtime.h` is untouched by
   this port (`git diff fba2565..HEAD -- include/Giri/Runtime.h` is empty), so
   the layout and the size-divides-page-size invariant are preserved; re-checked
   in-container on 14.0.0: `sizeof(Entry) == 32`, `4096 % 32 == 0`.
3. **Debug info** — verified. The harness compiles every test with `-g`
   (`CFLAGS += -g -O0 -c -emit-llvm`); `SourceLineMapping` reads the DWARF
   `DI` metadata. Re-checked in-container: `clang -g` on 14.0.0 emits
   `.debug_info`/`.debug_line`, and the resulting `file:line` slices match the
   3.4-era goldens (source-line based) across all 22 passing tests — e.g.
   test1's slice prints `Source Line Info: /giri/test/UnitTests/test1/
   extlibcalls.c:12` and the extracted list is `9 12 13 18` (== golden).

## Messages in passing tests

- **Program stdout** (e.g. `MingliangLIU`/`12`, matrix prints, `Final Means:`,
  `MatrixMult: Running...`) — expected output of the traced binaries. Captured
  per test in `_test_logs/` (copied into this audit dir) and quoted in each
  per-test report.
- **`Start slicing Function:Instruction is defined as <fn>:<n>` /
  `Start slicing Filename:Loc is defined as <file>:<line>`** — routine
  per-criterion progress logging from `Giri.cpp`. Not a finding.
- **`Statistics are disabled. Build with asserts or with
  -DLLVM_FORCE_ENABLE_STATS`** — the prebuilt 14.0.0 Release `opt` prints this
  when the harness passes `-stats`; benign, does not affect the slice output
  (it goes to stderr, and the harness captures the slice from
  `-slice-file`). Not a finding. (Unlike the legacy-PM run — where the
  identical toolchain printed it the same way — this is a harness-visible
  stderr line, not a behavior change.)

## Non-suite test directories (excluded, same as the 8.0.0 / legacy-PM audits)

test6/test7 (signals), test22 (fp/`-lm`), HelloWorld, histogram,
linear_regression, word_count — none wired into `test/auto-tests.txt`.
(HelloWorld's `Makefile` *was* updated to the new-PM harness, so it remains
runnable by hand; it is not in the automated suite.)

## Suite results across the port

| LLVM | Suite result (seq, honest harness) | Standing failures |
|------|------------------------------------|-------------------|
| 5.0.2 | 21 PASS / 1 FAIL (audit at `porting/TestAudit/llvm-5.0.2/`) | matrix_multiply-seq (FAIL-EXPECTED drift at `:292`) |
| 8.0.0 | 22 PASS / 0 FAIL (audit at `porting/TestAudit/llvm-8.0.0/`) | none in the automated suite |
| 14.0.0 (legacy PM) | 22 PASS / 0 FAIL (audit at `porting/TestAudit/llvm-14.0.0-legacypm/`) | none in the automated suite |
| **14.0.0 (new PM)** | **22 PASS / 0 FAIL (this audit)** | none in the automated suite |

### Standalone `tracer` tool (outside the suite)

The automated suite drives every pipeline stage through `opt`, whose
`PassBuilder` auto-registers the built-in analyses. The standalone `tracer`
tool builds its pipeline and analysis manager **by hand**, so it must register
the built-in analyses itself; this was **not** covered by the suite and was
verified separately after the initial 22/22:

- First end-to-end run (instrument → llc → link → run → slice) **segfaulted**:
  14.0.0's `ModulePassManager::run` fetches `PassInstrumentationAnalysis` from
  the MAM before running any pass, and `VerifierPass::run` fetches
  `VerifierAnalysis`. In a hand-built MAM those are unregistered, and with
  assertions compiled out (prebuilt Release toolchain) that is a silent
  null-deref, not a catchable assert.
- Fix (in `tools/Tracer/Tracer.cpp`): register
  `PassInstrumentationAnalysis` + `VerifierAnalysis` on the MAM — the exact
  analyses this pipeline consumes.
- After the fix: the test1 round-trip runs clean and the slice's source lines
  (`9 12 13 18`) match the pristine 3.4 golden `ans-inst.txt` exactly.
  The match is proven to consume a real trace, not a false positive: the
  trace file is non-empty (1216 bytes, 4 lines), and a negative control
  (slicing with a *missing* trace file) aborts on the `TraceFile` ctor
  assertion `(fd > 0) && "Cannot open file!"` (rc 134, 0-byte slice), so a
  golden match is impossible without a genuinely written+read trace.
  (Note: the runtime's default trace filename is `bbrecord`; the harness and
  this check pass an explicit `-trace-file`.)

## Known residuals (this port)

The new-PM port is functionally closed (22/22 on the honest seq suite). The
residuals are the same inherited gaps as the legacy-PM port, plus one
forward-compat note:

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| Legacy pass manager deprecated in 14.0.0 | **This branch is the forward-compatible answer** | The legacy-PM branch (`port/llvm-14.0.0-legacypm`) carries the deprecation risk; this branch (`port/llvm-14.0.0`) executes every pass through the new PM, so it is the variant that survives LLVM 15+ where the legacy PM is removed. No `-enable-new-pm=0` anywhere. |
| `opt` needs `-load` + `-load-pass-plugin` for each plugin | New-PM-specific, inherent | 14.0.0's new-PM driver only discovers passes via `-load-pass-plugin`; the plugin's `cl::opt` globals still need the plain `-load` (pre-parse dlopen). The harness documents the double-load. Inherent to 14.0.0's plugin loading, not a bug. |
| Pthread variants not re-measured on 14.0.0 | [inherited] gap (suite scope) | `Dockerfile` pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0 audit's. Same scope as 5.0.2/8.0.0/legacy-PM. |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0/legacy-PM. |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite (HelloWorld's Makefile was updated to the new-PM harness so it runs by hand). |
| `ensurePostDomFrontierComputed` — memory leak (legacy) | Replaced / [inherited] benign | The legacy lazy-wrapper hack (`new PostDominatorTreeWrapperPass`, freed by neither) is replaced: `DynamicGiri` builds the `PostDominatorTree` inline in `ensurePostDomFrontierComputed` (public `Function&` ctor, spike-verified to build and to support `properlyDominates` on the node form). The bounded `new PostDominanceFrontier`/tree per function is still not freed — same `opt`-process-lifetime bound as the legacy port, no observable cost. |
| `signal(SIGKILL, …)` — no-op | [inherited] harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX. Inherited from 5.0.2/8.0.0/legacy-PM. |
