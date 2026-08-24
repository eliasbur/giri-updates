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

| N | slice lines | diff lines | note |
|---|-------------|------------|------|
| 120–122 | 51 | 9–11 | |
| 123 | 57 | 3 | |
| 124–126 | 51 | 9 | |
| 127 | 52 | 8 | |
| 128–130 | 51 | 9 | |
| 131 | 52 | 8 | |
| 132–135 | 51–52 | 8–9 | |
| **136** | **58** | **2** | **closest to the 60-line golden** |
| 137 | 51 | 11 | |
| **138** | **50** | **10** | **the 3.4-era criterion** |
| 139–141 | 50 | 10 | |
| 142–144 | 49 | 13 | |
| 145 | 48 | 12 | |
| 146–148 | 42 | 20 | |

The N=136 slice differs from the golden by exactly lines `102, 103`
(the `if (i == num_procs-1) out->length = ...` tail assignment at the end of the
thread-spawn loop in `map`):
```
19a20,21
> 102
> 103
```
So the 8.0.0-codegen golden-equivalent criterion is approximately `matrixmult_map:136`
(with a residual 2-line diff on the tail-assignment branch), not `:138`. The residual
diff is because the criterion instruction's data-flow reachability at 136 does not cover
the cross-iteration `out->length` write at lines 102–103 under 8.0.0 codegen — the same
class of per-instruction reachability difference that produced the +7 drift on the
5.0.2 seq variant.

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

## Proposed resolution (needs a decision, per the golden-file constraint)

Two options, both consistent with the 5.0.2 precedent:
1. **Retune the criterion file** `test/matrix_multiply/criterion-inst-pthread.txt`
   from `matrixmult_map 138` to the 8.0.0-codegen equivalent (the sweep suggests `:136`
   with a residual 2-line diff, or a finer search around it). This changes the
   *criterion*, not the golden — the 5.0.2 port did exactly this for the seq variant.
2. **Leave as-is** and document the FAIL-EXPECTED pthread drift in the PR, since the
   pthread variant is out of scope for the automated suite and the 5.0.2 port also
   documented (rather than retuned) the seq FAIL-EXPECTED at first.

Per the user constraint ("do NOT change test cases (golden files, `ans-*.txt`) without
explicit user consent"), neither option has been applied; both are deferred to a user
decision.
