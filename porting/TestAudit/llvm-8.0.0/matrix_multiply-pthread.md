# matrix_multiply-pthread

- **Verdict:** FAIL-EXPECTED (criterion instruction drift, not in the automated suite)
- **Variant:** pthread (`TEST_PARALLELISM=pthread`, run manually; the Dockerfile pins `seq` for the automated suite)
- **Golden file:** ans-inst-pthread.txt (60 lines)   **Criterion:** `matrixmult_map 138` (criterion-inst-pthread.txt)
- **Diff:** 10 lines from golden absent in actual; 0 lines extra (50 of 60 preserved)
- **Root cause:** criterion instruction drift — the same kind of drift documented for
  `matrix_multiply-seq` on the 5.0.2 port. Under LLVM 8.0.0 codegen, `matrixmult_map`
  contains 148 instructions (3.4-era goldens were generated under LLVM 3.4 codegen), so
  3.4's `matrixmult_map:138` now resolves to a different instruction. No golden line is
  wrong or extra; 10 golden lines (102, 103, 140, 144, 145, 147, 149, 150, 152, 155) are
  simply not backward-reachable from the drifted criterion instruction, so they are
  missing from the slice.

## Criterion instruction identification (8.0.0 codegen)

`matrixmult_map` has **148** instructions under LLVM 8.0.0 (counted the way Giri counts,
i.e. over the function body with `dbg`-annotated statements; see the sweep below for the
slice-output sensitivity).

**Current criterion — instruction #138 (8.0.0):**
```
%114 = load i32, i32* %4, align 4, !dbg !352
```
(a load inside the inner product loop; `!dbg !352` maps to the `value += ...` /
`b_ptr += ...` region, source lines ~149–150)

## Criterion sweep (8.0.0, `matrixmult_map:N`, diff against the 60-line golden)

**Full function sweep, N = 1..148 (the complete 8.0.0 instruction count):**

- **No index exactly reproduces the 60-line golden.**
- **N = 136 is the best (58/60; the only residual is lines 102/103).**
- Runners-up: N = 123 (57/60), N = 100/101/102 (56/60), N = 99 (55/60).
- N = 138 (the 3.4-era criterion): 50/60, 10 lines missing.
- **N = 31, 32, 43, 44 abort** (SIGABRT) with the assertion
  `TraceFile.cpp:86 getLastDynValue: "Cannot find instruction in trace!"` —
  pre-existing Giri behavior for criteria that reference an instruction never
  executed in the trace (a `map`-function instruction in a 256-thread run);
  this is a property of the criterion choice, not a port regression.

**N=136 diff vs golden (the residual 2 lines):**
```
19a20,21
> 102
> 103
```
Lines 102–103 are the `if (i == num_procs-1) out->length = ...` tail assignment at
the end of the thread-spawn loop in `map` — a cross-iteration write that no
single `matrixmult_map`-body instruction's backward slice covers under 8.0.0
codegen. The same class of per-instruction reachability difference that produced
the +7 drift on the 5.0.2 seq variant.

## Why this is FAIL-EXPECTED and not a port bug

- The slice is **monotonic-subset** of the golden: no golden line is wrong, and no
  non-golden line appears. All 50 present lines are correct source lines for the slice.
- The criterion is an **instruction index** (`matrixmult_map 138`), which is
  codegen-dependent: LLVM 3.4, 5.0.2, and 8.0.0 each emit a different number of
  instructions in `matrixmult_map`, so the same index points at a different instruction
  (and therefore a different backward-reachable set).
- The same mechanism was documented for `matrix_multiply-seq` on the 5.0.2 port
  (drift +7, 3.4's `#285` → 5.0.2's `#292`), and the 5.0.2 port chose to **retune the
  criterion file** (commit `ec0e6b7`) rather than regenerate the golden. The 8.0.0 seq
  variant does **not** need a new retune (the 5.0.2 retuned `:291` still matches the
  19-line golden exactly on 8.0.0), but the pthread variant's `:138` was never retuned
  (it is not in the automated suite) and does not reproduce on 8.0.0.
- The pthread variant is **not in `auto-tests.txt`**, so this does not affect the
  21/21 automated-suite result.

## Slicing-stage output

`Start slicing Function:Instruction is defined as matrixmult_map:138` followed by a
normal slice; no "Could not find Control-dep" warnings, no assertion, no fatal error.
The exit code is non-zero only because the `diff` at stage 10 fails (the golden
mismatch), not because any stage crashed.

## Resolution

The **full** 1–148 sweep (above) is definitive: under 8.0.0 codegen no single
`matrixmult_map:N` instruction reproduces the 60-line 3.4 golden, so the
5.0.2-style criterion retune (the fix used for the seq variant, `ec0e6b7`) has
**no exact equivalent here**. The closest index (`:136`, 58/60) still leaves a
residual diff (lines 102/103), so retuning would not make the variant pass the
harness `diff` — it would only move the failure. The residual is structural:
the golden's lines 102/103 are a cross-iteration write in `map` that no
`matrixmult_map`-body instruction's slice covers under 8.0.0 codegen.

Verdict: **FAIL-EXPECTED stands as the final, evidence-backed state.** The
pthread variant is out of the automated suite, the slice it does produce is a
correct monotonic subset (no wrong lines), and the only test-file changes
across the port lineage remain the pre-existing 5.0.2 seq criterion retune and
this untouched criterion.

If a future pass wants the pthread variant green on 8.0.0, the honest options
are: (a) regenerate `ans-inst-pthread.txt` from a chosen 8.0.0-codegen criterion
(a golden regeneration — requires explicit user consent per the project
constraint), or (b) run the variant on a pinned-CPU host (e.g.
`--cpuset-cpus=0-3`) so `matrixmult_map`'s codegen is closer to the 3.4-era
one the golden was generated under. Neither is applied here.

Per the user constraint ("do NOT change test cases (golden files, `ans-*.txt`)
without explicit user consent"), no test file was changed.
