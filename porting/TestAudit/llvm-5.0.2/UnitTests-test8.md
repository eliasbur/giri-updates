# test8

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 40 54   **Criterion:** (none — default criterion)
- **Diff:** 1 line missing (line 12) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees, leaving the frontier map empty for `main` and causing line 12's control-dep branch to be excluded from the slice.

## What the does
`ptr.c` implements a `swap()` function via pointers and a `main` that reads two integers from argv, swaps them if `a < b`, and returns the larger value (`a`). Line 12 (`if (argc < 2) {`) is a guard that prints usage and exits if insufficient arguments are provided. The criterion (no explicit criterion file) defaults to the return instruction of `main` at line 24 (`return a;`). The slice should include line 12 because the branch at line 12 controls whether execution reaches the parsing and swapping paths. With input `40 54`, `a=40 < b=54` so `swap(&a, &b)` executes, and the program returns 54.

## Stage-by-stage output

### Stage 1 — make clean
Silent (expected).

### Stage 2 — clang -emit-llvm ptr.c → ptr.bc
Silent (expected).

### Stage 3 — llvm-link ptr.bc → ptr.all.bc
Silent (expected).

### Stage 4 — opt -trace-giri ptr.all.bc → ptr.trace.bc
Silent on both stdout and stderr (expected).

### Stage 5 — llc ptr.trace.bc → ptr.trace.s
Silent (expected).

### Stage 6 — clang++ ptr.trace.s → ptr.trace.exe
Silent (expected).

### Stage 7 — ./ptr.trace.exe 40 54
- stdout: silent
- stderr: silent
- exit status: 54 (correct — `a=40 < b=54`, so swap occurs, and `a` becomes 54)

### Stage 8 — opt -dgiri ptr.all.bc → ptr.slice
- stdout: silent
- stderr:
  ```
  Could not find Control-dep of this Basic Block 
  Could not find Control-dep of this Basic Block 
  ```
  (emitted from `lib/Giri/Giri.cpp:153`, two occurrences)

### Stage 9 — sed/awk/sort/uniq → ptr.slice.loc
Output (3 lines): 6 18 24. No errors.

### Stage 10 — diff ptr.slice.loc ans-inst.txt
Diff output:
```
1a2
> 12
EXIT:1
```
Line 12 is in the golden file but absent from the computed slice.

## Diff against golden
Golden (4 lines): 6 12 18 24
Computed (3 lines): 6 18 24

Missing line: 12 (`if (argc < 2) {` in `main`).

This instruction is in the entry block of `main` and its branch controls whether execution reaches the argument-parsing path (lines 17-18) and ultimately the return at line 24. It should appear as a control-dependent instruction in the backward slice. It is absent because `findExecForcers` returned an empty set for blocks in `main`, causing `Trace->getExecForcer` to return `nullptr` (`Giri.cpp:150`), which triggered the `"Could not find Control-dep of this Basic Block"` warning twice (`Giri.cpp:153`) and skipped including the branch condition.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) contains `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. In contrast, the original 3.4 code (on `master`) has `if (getRoots().empty()) return S;` followed by `if (BB) { …pred loop… }` — the null-block check guards only the predecessor loop, and the recursive call `calculate(DT, IDominee)` (line 48) runs for all children regardless.
   - **Why it matters here:** `main` has two exits: one path calls `exit()` (line 14), the other path `ret`s (line 24). LLVM 5.0.2's `PostDominatorTree` creates a *virtual root* node whose `getBlock()` returns `nullptr` for such functions. The port's early return at line 37 exits `calculate` before recursing into children, leaving the `Frontiers` map empty for the entire `main` function.
   - **Chain to the symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set for every block in `main` → `findExecForcers` (`Giri.cpp:86-98`) populates nothing in `ForceExecCache` → `getExecForcer` returns `nullptr` → two warnings at `Giri.cpp:153` (one for each non-entry BB in `main` that needs control-dep resolution) → branch at line 12 never added to worklist → line 12 absent from slice output.
   - **Confirmed by:** Same root cause as `UnitTests-test3.md`. The two occurrences of the warning correspond to two distinct basic blocks in `main` (the argc-check block and the `if (a < b)` conditional block) where `getExecForcer` returns `nullptr`.

2. **stderr warnings from `lib/Giri/Giri.cpp:153`.**
   - **Verdict:** FAIL-BUG (symptom of root cause 1).
   - **Evidence:** `errs() << "Could not find Control-dep of this Basic Block \n"` — emitted during the `-dgiri` pass. Two occurrences correspond to two basic blocks in `main` whose control dependencies cannot be resolved due to the empty frontier map. Direct consequence of root cause 1.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard that only wraps the predecessor loop (lines 39-44), mirroring the original 3.4 structure. This allows the child recursion at lines 46-55 to run even when the current node is the virtual root with a null block. Intended behaviour: frontiers for non-null children of the virtual root are computed, enabling correct control-dependence lookup for blocks in functions with multiple exits. See also `UnitTests-test3.md` for identical root cause analysis.