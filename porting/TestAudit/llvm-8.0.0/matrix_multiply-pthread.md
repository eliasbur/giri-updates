# matrix_multiply-pthread

- **Verdict:** FAIL-EXPECTED (criterion instruction drift, not in the automated suite)
- **Variant:** pthread (`TEST_PARALLELISM=pthread`, run manually; the Dockerfile pins `seq` for the automated suite)
- **Golden file:** ans-inst-pthread.txt (60 lines)   **Criterion:** `matrixmult_map 138` (criterion-inst-pthread.txt)
- **Diff at the shipped criterion (`:138`):** 52 of 60 golden lines present; **0 extra** (monotonic subset). The 8 absent golden lines are **140, 144, 145, 147, 149, 150, 152, 155**.
- **Root cause:** criterion instruction drift — the same class documented for `matrix_multiply-seq` on the 5.0.2 port. `matrixmult_map` contains **153** instructions under 8.0.0 codegen (154 in the module actually fed to `-dgiri`, i.e. after `-mergereturn -bbnum -lsnum`; counted exactly as `Giri.cpp:300` counts them, via `inst_begin`/`inst_end`). 3.4's `matrixmult_map:138` therefore resolves to a different instruction than the one the golden was cut from.

## Corrected criterion sweep (8.0.0, `matrixmult_map:N`, diff against the 60-line golden)

> **Correction note (2026-08-24).** The first version of this report (commit `15e80aa`) recorded a
> function count of 148 and a sweep of N=1..148 concluding "no index reproduces the 60-line
> golden exactly; N=136 closest at 58/60". A faithful re-run from a clean rebuild
> (fresh `build/`, fresh traced binary, exact `Makefile.common` commands, a small
> `inst_begin`/`inst_end` counter compiled against LLVM 8.0.0) showed the earlier pass had
> run against a stale/different build state. The corrected facts are below and are reproducible.

**Exhaustive function sweep, N = 1..154 (the full 8.0.0 instruction count):**

- **N = 136 reproduces the 60-line golden EXACTLY (60/60, 0 missing, 0 extra).** This is a
  valid criterion retune and is **reproducible** — verified across 5 independently
  regenerated traces (the pthread trace differs run-to-run; see non-determinism note).
- N = 138 (the 3.4-era criterion): 52/60, monotonic subset (0 extra); 8 golden lines absent
  (140, 144, 145, 147, 149, 150, 152, 155).
- Runners-up near the target: N = 123 (59/60, 0 extra), N = 100/101/102 (58/60, 0 extra),
  N = 99 (57/60, 0 extra).
- **N = 31, 32, 43, 44 crash** (SIGSEGV, exit 139) inside the pre-existing, port-untouched
  `TraceFile::findPreviousID` path (the earlier report described a nearby
  `getLastDynValue` assert; the crash site in this build is `findPreviousID`, same
  pre-existing code, 0 lines of the port diff touch it). This is a property of criterion
  choice (a `matrixmult_map` instruction not fully executed in a 4-thread trace), not a
  port regression. N = 154 also segfaults (last instruction, no preceding trace record).

**N=138 diff vs golden (the residual 8 lines — present in the golden, absent from the
`:138` slice):**
```
140
144
145
147
149
150
152
155
```
(8 source lines; 60 − 8 = 52 present, 0 extra — a monotonic subset.)

## Why this is FAIL-EXPECTED and not a port bug

- At the shipped `:138` the slice is a **monotonic subset** of the golden: all 52 present
  lines are correct source lines; no non-golden line appears. 8 golden lines are not
  backward-reachable from the drifted criterion instruction under 8.0.0 codegen, so they
  are missing from the slice.
- The criterion is an **instruction index** (`matrixmult_map 138`), which is
  codegen-dependent: LLVM 3.4, 5.0.2, and 8.0.0 each emit a different number of
  instructions in `matrixmult_map`, so the same index points at a different instruction
  (and therefore a different backward-reachable set).
- The same mechanism was documented for `matrix_multiply-seq` on the 5.0.2 port (drift +7,
  3.4's `#285` → 5.0.2's `#292`), and the 5.0.2 port chose to **retune the criterion file**
  (commit `ec0e6b7`) rather than regenerate the golden. The 8.0.0 seq variant does **not**
  need a new retune (the 5.0.2 retuned `:291` still matches the 19-line golden exactly on
  8.0.0). The pthread variant's `:138` was never retuned (it is not in the automated suite)
  and does not reproduce on 8.0.0 **as shipped** — but, unlike the seq case, a valid
  retune **does** exist here (see Resolution).
- The pthread variant is **not in `auto-tests.txt`**, so this does not affect the
  21/21 automated-suite result.

## Trace non-determinism (affects any per-run pthread measurement)

The traced pthread binary produces a **different `.trace` file on every run** (3 independent
runs → 3 distinct trace md5s, identical byte count). The dynamic backwards slice therefore
varies with the thread interleaving for a fixed static criterion index. The `:136`
exact-match and the `:138` 52/60-subset results were each stable across the 5 fresh-trace
runs above, but per-run pthread slice output is an inherent property of the tool, not a
port regression; the `seq` variant (deterministic single-thread trace) is the one with
meaningingly stable criterion-sweep results.

## Slicing-stage output

`Start slicing Function:Instruction is defined as matrixmult_map:138` followed by a
normal slice; no "Could not find Control-dep" warnings, no fatal error. The exit code is
non-zero only because the `diff` at stage 10 fails (the golden mismatch), not because any
stage crashed.

## Resolution (corrected)

The exhaustive 1–154 sweep shows that, **unlike the seq variant, a valid criterion retune
exists for the pthread variant: `matrixmult_map 136` reproduces the 60-line golden exactly
(60/60, 0 missing, 0 extra), reproducibly across fresh traces.** The 5.0.2-style fix for the
seq variant (retuning the criterion file, commit `ec0e6b7`) has an exact equivalent here:
changing `test/matrix_multiply/criterion-inst-pthread.txt` from `matrixmult_map 138` to
`matrixmult_map 136` would make the variant pass the harness `diff`.

**No test file was changed in this port.** Per the user constraint ("do NOT change test
cases, especially golden files / `ans-*.txt`, without explicit user consent"), the
criterion file is left untouched; the retune is offered as the ready-made forward option
(option (a) below) and requires explicit user sign-off before it is applied.

If a future pass wants the pthread variant green on 8.0.0, the honest options are:
(a) retune `criterion-inst-pthread.txt` to `matrixmult_map 136` (a criterion-file edit —
requires explicit user consent per the project constraint; the golden is **not** changed),
or (b) run the variant on a pinned-CPU host (e.g. `--cpuset-cpus=0-3`) so the
interleaving is closer to the one under which the golden was generated. Neither is applied
here.

Verdict: **FAIL-EXPECTED stands as the final, evidence-backed state for the *shipped*
criterion (`:138`).** The correction to the earlier "no exact retune exists" claim is
recorded above; the corrected conclusion is that a valid retune (`:136`) exists and is the
ready-made forward option, held behind the user-consent constraint.
