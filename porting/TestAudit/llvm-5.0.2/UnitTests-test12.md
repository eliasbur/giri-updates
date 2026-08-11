# test12

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 16 8   **Criterion:** (none — CRITERION empty)
- **Diff:** 3 lines missing (20, 28, 44) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees, emptying the frontier map for `main` (which has multiple exits: `exit()` and `return`), causing 32 "Could not find Control-dep" warnings at `Giri.cpp:153` and excluding lines 20, 28, and 44 from the slice.

## What the test does
`psum.c` is a pthread-based parallel summation program (CS:APP §12.6). `main` parses two arguments (`nthreads=16`, `log_nelems=8`), computes `nelems = 1 << 8 = 256`, the expected sum `esum = 256 * 255 / 2 = 32640`, and `nelems_per_thread`. It spawns 16 threads, each running `sum()` which computes a partial sum over its assigned range and stores it in the shared `psum[]` array. After joining all threads, main accumulates partial sums, validates against `esum`, prints the result, and returns `result % 31`. The test exercises shared variables, for loops, arrays, and pthreads. With input `16 8`, the program prints `The supposed sum is: 32640` and exits with code `32640 % 31 = 25`.

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm psum.c -o psum.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link psum.bc -o psum.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... psum.all.bc -o psum.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 psum.trace.bc -o psum.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing psum.trace.s -o psum.trace.exe -L/giri/build/lib -lrtgiri -pthread`
No output (stdout or stderr).

### Stage 7: `./psum.trace.exe 16 8`
- stdout: `The supposed sum is: 32640`
- stderr: empty
- Program exits normally; the trace file `psum.trace` is generated with records from 17 threads (1 main + 16 worker threads).

### Stage 8: `opt ... -dgiri ... psum.all.bc -o /dev/null`
- stdout: empty
- stderr: Extensive warnings:
  - 32 occurrences of `Could not find Control-dep of this Basic Block` (from `lib/Giri/Giri.cpp:153`)
  - 8 occurrences of `For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later` followed by `i8* %0`

  The first burst contains 18 "Could not find Control-dep" messages (from `main`'s basic blocks). This is followed by 8 groups of (2 "Could not find Control-dep" + 1 "call records missing" + 1 "i8* %0"), corresponding to basic blocks in `sum()` across the 16 pthread worker threads (8 unique function instances traced).

### Stage 9: `sed ... psum.slice | awk ... | sort -g | uniq > psum.slice.loc`
No errors. Generated `psum.slice.loc` with 17 lines:
`15 25 26 27 29 32 33 40 41 51 56 57 58 59 61 62 64`

### Stage 10: `diff psum.slice.loc ans-inst.txt`
Diff output:
```
1a2
> 20
4a6
> 28
9a12
> 44
```
Three lines present in the golden file but absent from the computed slice: 20, 28, 44.

## Diff against golden

Golden (20 lines): 15, 20, 25, 26, 27, 28, 29, 32, 33, 40, 41, 44, 51, 56, 57, 58, 59, 61, 62, 64
Computed (17 lines): 15, 25, 26, 27, 29, 32, 33, 40, 41, 51, 56, 57, 58, 59, 61, 62, 64

Missing lines:
- **20**: `if (argc != 3) {` — input-argument-count guard in `main`. This branch controls whether execution proceeds to the argument-parsing path. Should appear as a control-dependent instruction since the branch determines whether `main` reaches the summation logic.
- **28**: `esum = nelems * (nelems - 1) / 2;` — expected sum computation in `main`. The variable `esum` flows into line 44's comparison check, which then controls the final return value. Missing because the block containing line 28 cannot resolve its control dependency due to the empty frontier map.
- **44**: `if (result != esum) {` — final result validation in `main`. This branch controls whether the program calls `exit(EXIT_FAILURE)` or proceeds to `printf` and `return`. Should be in the slice as it directly controls the criterion location (line 51: `return result % 31`).

All three missing lines are in `main`, which has multiple exit paths (`exit(EXIT_FAILURE)` from lines 22 and 46, and `return` at line 51). This makes `main`'s PostDominatorTree use a virtual root node, triggering the `PostDominatorFrontier.cpp:37` bug.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) contains `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. The original 3.4 code on `master` guards only the predecessor loop with an `if (BB) { … }` block, allowing the recursive call `calculate(DT, IDominee)` (line 48) to execute for all children regardless of whether the current node is the virtual root.
   - **Why it matters here:** `main` has three exit paths: `exit()` at line 22, `exit()` at line 46, and `return` at line 51. LLVM 5.0.2's `PostDominatorTree` creates a *virtual root* node whose `getBlock()` returns `nullptr`. The port's early return at line 37 exits `calculate` before recursing into children, leaving the `Frontiers` map empty for the entire `main` function.
   - **Chain to the symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set for every block in `main` → `findExecForcers` populates nothing in `ForceExecCache` → `getExecForcer` returns `nullptr` → "Could not find Control-dep" warnings at `Giri.cpp:153` → branch conditions for lines 20, 28, and 44 never added to worklist → these lines absent from slice output.
   - **Confirmed by:** Same root cause as `UnitTests-test3.md` and `UnitTests-test8.md`. The 32 "Could not find Control-dep" warnings correspond to basic blocks across both `main` and `sum` functions where `getExecForcer` returns `nullptr` due to the incomplete frontier map.

2. **"For some variable length functions like ap_rprintf in apache, call records missing" + `i8* %0`**
   - **Verdict:** PASS-NOISY (not responsible for the diff).
   - **Evidence:** 8 occurrences in stage 8 stderr. These come from `lib/Giri/TraceFile.cpp` when the trace parser encounters `i8*` pointer operations (from `pthread_create`'s `i8*` argument passing) without matching call/return records in the trace. This is a known limitation of the tracing infrastructure with pthread callbacks. Does not affect the correctness of the slice output.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard that wraps only the predecessor loop (lines 39-44). This allows the child recursion at lines 46-55 to execute for the virtual root's children, computing correct frontiers for blocks in functions with multiple exits. Intended behaviour: `Frontiers` map populated for all blocks in `main` → `getExecForcer` resolves control dependencies → lines 20, 28, 44 included in slice → diff empty. See also `UnitTests-test8.md` for identical root cause analysis.