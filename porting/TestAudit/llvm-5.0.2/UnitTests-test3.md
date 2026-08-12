# test3

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 15   **Criterion:** (none — default criterion)
- **Diff:** 1 line missing (line 17) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees, leaving the frontier map empty for `main` and causing line 17's control-dep branch to be excluded from the slice.

## What the test does
`fibonacci.c` computes the nth Fibonacci number recursively. `main` parses `n` from argv, calls `fibonacci(n)`, prints the result, and returns it. Line 17 contains `if (argc < 2)` — a guard that exits with an error if no argument is provided. The criterion (no explicit criterion file) defaults to the return instruction of `main` at line 26 (`return f;`). The slice should include line 17 because the branch at line 17 controls whether execution reaches the fibonacci computation path.

## Stage-by-stage output

### Stage 1 — make clean
Silent (expected).

### Stage 2 — clang -emit-llvm fibonacci.c → fibonacci.bc
Silent (expected).

### Stage 3 — llvm-link fibonacci.bc → fibonacci.all.bc
Silent (expected).

### Stage 4 — opt -trace-giri fibonacci.all.bc → fibonacci.trace.bc
Silent on both stdout and stderr (expected).

### Stage 5 — llc fibonacci.trace.bc → fibonacci.trace.s
Silent (expected).

### Stage 6 — clang++ fibonacci.trace.s → fibonacci.trace.exe
Silent (expected).

### Stage 7 — ./fibonacci.trace.exe 15
- stdout: `fibonacci(15) is 610`
- exit status: 98 (610 mod 256, correct — the function returns `f`, and `fibonacci(15) == 610`)

### Stage 8 — opt -dgiri fibonacci.all.bc → fibonacci.slice
- stdout: silent
- stderr: `Could not find Control-dep of this Basic Block` (emitted from `lib/Giri/Giri.cpp:153`)

### Stage 9 — sed/awk/sort/uniq → fibonacci.slice.loc
Output (8 lines): 7 8 9 12 13 22 23 26. No errors.

### Stage 10 — diff fibonacci.slice.loc ans-inst.txt
Diff output:
```
5a6
> 17
EXIT:1
```
Line 17 is in the golden file but absent from the computed slice.

## Diff against golden
Golden (9 lines): 7 8 9 12 13 17 22 23 26
Computed (8 lines): 7 8 9 12 13 22 23 26

Missing line: 17 (`if (argc < 2) {` in `main`).

This instruction is in the entry block of `main` and its branch (`br i1 %9, label %10, label %16, !dbg !47`) controls whether execution reaches blocks containing lines 22, 23, 26. It should appear as a control-dependent instruction in the backward slice. It is absent because `findExecForcers` returned an empty set for blocks in `main`, causing `Trace->getExecForcer` to return `nullptr` (`Giri.cpp:150`), which triggered the `"Could not find Control-dep of this Basic Block"` warning (`Giri.cpp:153`) and skipped including the branch condition.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) contains `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. In contrast, the original 3.4 code (on `master`) has `if (getRoots().empty()) return S;` followed by `if (BB) { …pred loop… }` — the null-block check guards only the predecessor loop, and the recursive call `calculate(DT, IDominee)` (line 48) runs for all children regardless.
   - **Why it matters here:** `main` has two exits: one path calls `exit()`/`unreachable` (line 20), the other path `ret`s (line 26). LLVM 5.0.2's `PostDominatorTree` creates a *virtual root* node whose `getBlock()` returns `nullptr` for such functions. The port's early return at line 37 exits `calculate` before recursing into children, leaving the `Frontiers` map empty for the entire `main` function.
   - **Chain to the symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set for every block in `main` → `findExecForcers` (`Giri.cpp:86-98`) populates nothing in `ForceExecCache` → `getExecForcer` returns `nullptr` → warning at `Giri.cpp:153` → branch at line 17 never added to worklist → line 17 absent from slice output.
   - **Confirmed by:** `git show master:lib/Utility/PostDominatorFrontier.cpp` preserves the recursive child walk for null-BB nodes; the port does not.

2. **stderr warning from `lib/Giri/Giri.cpp:153`.**
   - **Verdict:** FAIL-BUG (symptom of root cause 1).
   - **Evidence:** `errs() << "Could not find Control-dep of this Basic Block \n"` — emitted during the `-dgiri` pass. The message is the direct consequence of the empty frontier map described in root cause 1.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard that only wraps the predecessor loop (lines 39-44), mirroring the original 3.4 structure. This allows the child recursion at lines 46-55 to run even when the current node is the virtual root with a null block. Intended behaviour: frontiers for non-null children of the virtual root are computed, enabling correct control-dependence lookup for blocks in functions with multiple exits.