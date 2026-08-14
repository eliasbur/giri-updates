# matrix_multiply-seq

- **Verdict:** FAIL-EXPECTED
- **Golden file:** ans-inst-seq.txt   **Input:** 4   **Criterion:** matrix_mult 285
- **Diff:** 1 line from golden absent in actual / 16 lines extra in actual
- **Root cause:** Criterion instruction drift — LLVM 3.4's `matrix_mult:285` resolves to
  LLVM 5.0.2's `matrix_mult:292`, a drift of **+7** instructions, both inside the
  output-printing loop. 3.4's #285 was the `dprintf("\n")` at source line 97 (newline print).
  5.0.2's #285 is the `fprintf` at source line 94 (value print, `dprintf("%d  ", ...)`).
  The golden file `ans-inst-seq.txt` is exactly reproducible from index 292 (confirmed by sweep
  of 250–298). The current criterion at 285 reads `matrix_out`, pulling the full computation
  chain into the slice; the golden's criterion (`dprintf("\n")`) reads no computed data, so
  the computation chain is absent.

## Criterion instruction identification

`matrix_mult` has **298 instructions** under LLVM 5.0.2 (counted via `inst_iterator`, which
increments over `BasicBlock::iterator` and therefore includes `dbg.declare` intrinsics as full
instructions — confirmed by reading `InstIterator.h` from LLVM 5.0.2's headers).

**Current criterion — instruction #285 verbatim:**
```
%252 = call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %238,
  i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.3, i32 0, i32 0),
  i32 %251), !dbg !252
```

**`!dbg !252` → source line 94** (`dprintf("%d  ", data_in->matrix_out[(data_in->matrix_len)*i + j])`).

Line 94 is **not** among the golden's 19 lines (56, 90, 97, 113, 119, 120, 121, 122, 123, 129,
131, 133, 137, 139, 141, 145, 147, 149, 154).

**Golden-reproducing criterion — instruction #292 verbatim:**
```
%258 = call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %257, i8* getelementptr
  inbounds ([2 x i8], [2 x i8]* @.str.2, i32 0, i32 0)), !dbg !259
```

**`!dbg !259` → source line 97** (`dprintf("\n")`).

The `matrix_out` store (the computation result accumulation, `store i32 %199, i32* %197`)
is at instruction #224 (source line 84). The golden's criterion was **not** the computation
store — the golden omits line 84 entirely.

## Sweep results

A sweep of indices 250–298 was performed, comparing each criterion `matrix_mult:N` against
the golden file `ans-inst-seq.txt`. The full results:

| Index | Result | Lines | Notes |
|---|---|---|---|
| 250 | differs | 19 | |
| 251 | differs | 18 | |
| 252 | differs | 18 | |
| 253 | differs | 18 | |
| 254 | differs | 18 | |
| 255 | differs | 2 | |
| 256 | differs | 2 | |
| 257 | differs | 18 | |
| 258 | differs | 18 | |
| 259 | differs | 18 | |
| 260 | differs | 18 | |
| 261 | differs | 18 | |
| 262 | differs | 18 | |
| 263 | differs | 19 | |
| 264 | differs | 19 | |
| 265 | differs | 19 | |
| 266 | differs | 19 | |
| 267 | differs | 19 | |
| 268 | differs | 19 | |
| 269 | differs | 19 | |
| 270 | differs (2 lines) | 19 | |
| 271 | differs (3 lines) | 19 | |
| 272 | differs (3 lines) | 19 | |
| 273 | differs (3 lines) | 19 | |
| 274 | differs (4 lines) | 19 | |
| 275 | differs (3 lines) | 19 | |
| 276 | differs (3 lines) | 19 | |
| 277 | differs (3 lines) | 19 | |
| 278 | differs (3 lines) | 19 | |
| 279 | differs (3 lines) | 19 | |
| 280 | differs (3 lines) | 19 | |
| 281 | differs (3 lines) | 19 | |
| 282 | differs (3 lines) | 19 | |
| 283 | differs (4 lines) | 19 | |
| 284 | differs (17 lines) | 34 | |
| 285 | differs (17 lines) | 34 | current criterion (line 94) |
| 286 | differs (3 lines) | 19 | |
| 287 | differs (2 lines) | 19 | 97 out, 92 in |
| 288 | differs (2 lines) | 19 | 97 out, 92 in |
| 289 | differs (2 lines) | 19 | 97 out, 92 in |
| 290 | differs (2 lines) | 19 | 97 out, 92 in |
| **291** | **MATCH** | **19** | `load @stdout`, `!dbg !259` → line 97 |
| **292** | **MATCH** | **19** | `@fprintf("\n")`, `!dbg !259` → line 97 |
| 293 | differs (2 lines) | 19 | 97 out, 98 in |
| 294 | differs (1 line) | 18 | 97 missing |
| 295 | differs (1 line) | 18 | 97 missing |
| 296 | differs (1 line) | 18 | 97 missing |
| 297 | differs (1 line) | 18 | 97 missing |
| 298 | differs (19 lines) | 1 | line 99 only (end of function) |

**Two matches**: index 291 (`%257 = load @stdout, !dbg !259`, the argument load for line 97's
`dprintf("\n")`) and index 292 (`%258 = call @fprintf(..., `"\n"`), `!dbg !259`, the call
itself). Both resolve to source line 97. Index 292 is the criterion instruction proper.

The drift is **+7** (292 − 285 = 7), entirely within the output-printing loop. Both the golden's
criterion (line 97's `dprintf("\n")`) and the current criterion (line 94's `fprintf`
that prints the value) are in the same `for(j ...)` loop body.

## Pthread counter-argument

`matrix_multiply-pthread` has its own criterion: `matrixmult_map 138`. The `matrixmult_map` function
has 155 instructions total. Instruction #138 is:

```
store i32 %99, i32* %111, align 4, !dbg !311  → source line 155
```

This is the store to `matrix_out` in the computation loop. Source line 155 **is** in the pthread
golden (60 lines). The pthread golden matches exactly because its criterion targets the same
computation-store instruction across both LLVM 3.4 and 5.0.2 builds. The `matrixmult_map` function
is shorter (155 vs 298 instructions) and its criterion (#138) falls in the computation region
rather than drifting past the computation into the output-printing region.

**Why seq drifts but pthread doesn't:** The seq `matrix_mult` function (298 instructions) includes
large output-printing loops after the computation (instructions ~268-298), so instruction #285
lands past the computation store. The pthread `matrixmult_map` function (155 instructions) is
shorter and its criterion #138 lands solidly within the computation region, where the IR is the
same across compiler versions.

## Extra lines classification (control vs data dependence)

Ran slicing with `-trace-cd=false` to disable control-dependence tracing and compared the two
slices:

- **34 lines** in the full slice (with CD)
- **28 lines** in the data-dependence-only slice (without CD)
- **6 lines** present only with CD (control-dependence sourced): 56, 79, 113, 131, 139, 147
- **28 lines** present in both (data-dependence sourced)

**Of the 16 extra lines (in actual but not golden):**
- **DD-sourced extras (15):** 75, 76, 77, 80, 81, 82, 84, 85, 86, 92, 94, 155, 156, 157, 165
- **CD-sourced extras (1):** 79

The vast majority of extra lines (15/16) are pure data-dependence from the shifted criterion. The
criterion's `fprintf` at line 94 reads `matrix_out`, which traces through the full computation
accumulation chain back to the data setup in `main()` (lines 155-157, 165).

## Missing golden line 97 explained

Line 97 is `dprintf("\n")` — the newline print at the end of each output row. It was the LLVM 3.4
criterion itself (now confirmed to be index 292 in 5.0.2's function). The current criterion at
index 285 is the value-print `fprintf` at line 94. Two independent `fprintf` calls in the same BB or
adjacent BBs have no data dependence on each other: the newline call reads no computed data, and the
value call does not write to the newline call's arguments. Control dependence on the loop BB does not
reach from line 94's `fprintf` back to line 97's via `findExecForcers` (the criterion is in a
different trace iteration's BB).

## Line accounting

- **Golden lines present in actual output:** 18 (all except line 97)
- **Golden lines absent from actual output:** 1 (line 97)
- **Extra lines in actual output, not in golden:** 16
- **Total lines in actual slice:** 18 + 16 = **34**
- **Reported as 35 in original audit: corrected.** The original report's "35 unique source lines"
  was off by one.

18 (golden present) + 1 (golden missing) + 16 (extra) = 35 total lines, of which 34 are in the
actual slice (34 = 18 golden-present + 16 extra). Accounting is now consistent.

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm matrix_multiply-seq.c -o matrix_multiply-seq.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link matrix_multiply-seq.bc -o matrix_multiply-seq.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... matrix_multiply-seq.all.bc -o matrix_multiply-seq.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 matrix_multiply-seq.trace.bc -o matrix_multiply-seq.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing matrix_multiply-seq.trace.s -o matrix_multiply-seq.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./matrix_multiply-seq.trace.exe 4`
Stdout: 4×4 matrix printout + multiplication result (4×4 product matrix). Exit code 0.
Stderr: empty.

### Stage 8: `opt ... -dgiri ... matrix_multiply-seq.all.bc -o /dev/null`
Stderr: `Start slicing Function:Instruction is defined as matrix_mult:285`
No warnings. After DenseMap→std::map fix, 0 "Could not find Control-dep" messages (previously 13).

### Stage 9: `sed ... matrix_multiply-seq.slice | awk ... | sort -g | uniq > matrix_multiply-seq.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff matrix_multiply-seq.slice.loc ans-inst-seq.txt`
Non-empty diff. See "Diff against golden" below.

## Diff against golden

Actual output contains 34 unique source lines. Golden file contains 19 lines.

Lines in actual output but not in golden (16 extra): 75, 76, 77, 79, 80, 81, 82, 84, 85, 86, 92, 94, 155, 156, 157, 165. These correspond to:
- Lines 75-86, 92-94: matrix multiplication inner loop body (loop headers, boundary calculations, loads, multiplications, stores) and the output-printing `fprintf` at line 94 (which is the criterion instruction itself under LLVM 5.0.2).
- Lines 155-157, 165: main function data setup (pointer assignments to matrix_A, matrix_B, matrix_out; memset).

Line in golden but absent from actual (1 missing): 97 (`dprintf("\n")` — newline print in output formatting).

Every line in the golden that appears in the actual output (18 remaining golden lines) matches exactly.

## Root causes

1. **Criterion instruction drift between LLVM 3.4 and LLVM 5.0.2.** (FAIL-EXPECTED) The golden
   file `ans-inst-seq.txt` was generated against an LLVM 3.4 build. The golden is exactly
   reproducible from LLVM 5.0.2's `matrix_mult:292`, the `dprintf("\n")` call at source line 97
   (`!dbg !259`), confirming that 3.4's criterion #285 resolved to the same instruction. LLVM 5.0.2's
   function has 298 instructions total; #285 is now the value-print `fprintf` at source line 94
   (`!dbg !252`), **+7** instructions from the golden's criterion. Both are inside the
   output-printing loop. The backward slice from line 94's `fprintf` reads `matrix_out`, pulling the
   full computation chain (lines 75–86, 92, 155–157, 165) into the slice. The golden's criterion
   (`dprintf("\n")`) reads no computed data, so the computation chain is absent. The 1 missing line
   (97) is a separate `fprintf` call with no data or control dependence on line 94's call. The cause
   of the +7 instruction offset between LLVM 3.4 and 5.0.2 is not currently explained; the offset is
   small and entirely within one function's printing loop. Changing the golden file, criterion, or
   compiler version would be required to resolve it.

2. **DenseMap reference invalidation during recursive `calculate()`.** (fixed by this task) Before
   the fix, `Frontiers` was `DenseMap<BasicBlock*, DomSetType>`. The recursive `calculate()`
   function held a reference `DomSetType &S = Frontiers[BB]` across recursive calls that inserted
   new keys. DenseMap invalidates all references on rehash. For `matrix_mult`'s post-dominator
   tree (enough BBs to trigger rehash), this caused 13 "Could not find Control-dep" warnings and
   corrupted frontier data. The fix changed `Frontiers` to
   `std::map<BasicBlock*, DomSetType>`, which provides reference stability. After the fix, 0
   warnings and correct frontier computation. Evidence: with DenseMap, 13 "Could not find
   Control-dep of this Basic Block" messages appeared (captured in log); with std::map, 0 messages.
   Verified by reverting the fix and re-running: 13 warnings reappeared.

## Proposed fix
No further fix possible for root cause 1 (FAIL-EXPECTED — criterion instruction drift between
compiler versions). Root cause 2 is fixed by changing `Frontiers` from `DenseMap` to `std::map`
in `include/Utility/PostDominanceFrontier.h:28`.