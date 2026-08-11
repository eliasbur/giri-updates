# test9 (forloop)

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 6 19 7 17 32 35 40 54 302   **Criterion:** (none — default criterion)
- **Diff:** 1 line missing (line 9) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees, emptying the frontier map for `main` and causing the `if (argc < 2)` branch at line 9 to be excluded from the slice.

## What the test does
`forloop.c` reads integer arguments from the command line, computes their sum, minimum, and maximum, prints min/max, and returns the sum. Line 9 contains `if (argc < 2)` — a guard that calls `exit(EXIT_FAILURE)` if insufficient arguments are provided. The criterion defaults to the return instruction of `main` at line 25 (`return sum;`). The slice should include line 9 because the branch at line 9 controls whether execution reaches the loop and return paths.

## Stage-by-stage output

### Stage 1 — make clean
Silent on both stdout and stderr (expected).

### Stage 2 — clang -emit-llvm forloop.c → forloop.bc
Silent on both stdout and stderr (expected).

### Stage 3 — llvm-link forloop.bc → forloop.all.bc
Silent on both stdout and stderr (expected).

### Stage 4 — opt -trace-giri forloop.all.bc → forloop.trace.bc
Silent on both stdout and stderr (expected).

### Stage 5 — llc forloop.trace.bc → forloop.trace.s
Silent on both stdout and stderr (expected).

### Stage 6 — clang++ forloop.trace.s → forloop.trace.exe
Silent on both stdout and stderr (expected).

### Stage 7 — ./forloop.trace.exe 6 19 7 17 32 35 40 54 302
- stdout: `The min is 6, and the max is 302`
- stderr: silent
- exit status: 0

### Stage 8 — opt -dgiri forloop.all.bc → forloop.slice
- stdout: silent
- stderr: 28 occurrences of `Could not find Control-dep of this Basic Block` (emitted from `lib/Giri/Giri.cpp:153`)

### Stage 9 — sed/awk/sort/uniq → forloop.slice.loc
Output (4 lines): 14 15 20 25. No errors.

### Stage 10 — diff forloop.slice.loc ans-inst.txt
Diff output:
```
0a1
> 9
```
Line 9 is in the golden file but absent from the computed slice.

## Diff against golden
Golden (5 lines): 9 14 15 20 25
Computed (4 lines): 14 15 20 25

Missing line: 9 (`if (argc < 2) {` in `main`).

This instruction guards the early exit path. Its branch instruction controls whether execution reaches the loop (lines 14-21) and print/return (lines 23, 25). It should appear as a control-dependent instruction in the backward slice. It is absent because `findExecForcers` returned an empty set for blocks in `main`, causing `Trace->getExecForcer` to return `nullptr` (`Giri.cpp:150`), which triggered the `"Could not find Control-dep of this Basic Block"` warning (`Giri.cpp:153`) and skipped including the branch condition.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) has `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. The original 3.4 code guards only the predecessor loop with `if (BB) { … }` and recurses into children regardless. The virtual root in LLVM 5.0.2's `PostDominatorTree` causes early termination before recursing, leaving the `Frontiers` map empty.
   - **Why it matters here:** `main` has two exits: one calls `exit()` (line 11), the other `ret` (line 25). LLVM 5.0.2's `PostDominatorTree` creates a virtual root node with `getBlock() == nullptr`. The early return at line 37 exits `calculate` before recursing into children, leaving `Frontiers` empty for all blocks in `main`.
   - **Chain to symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set → `findExecForcers` populates nothing → `getExecForcer` returns `nullptr` → 28 warnings at `Giri.cpp:153` → branch at line 9 never added to worklist → line 9 absent from slice.

2. **stderr warnings from `lib/Giri/Giri.cpp:153`.**
   - **Verdict:** FAIL-BUG (symptom of root cause 1).
   - **Evidence:** 28 identical `errs() << "Could not find Control-dep of this Basic Block \n"` messages during `-dgiri`. Direct consequence of empty frontier map.

## Proposed fix
Same as test3. `lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard wrapping only the predecessor loop (lines 39-44), preserving the recursive child walk (lines 46-55) for virtual-root nodes. This allows frontiers of non-null children to be computed, enabling correct control-dependence lookup for all blocks in `main`.