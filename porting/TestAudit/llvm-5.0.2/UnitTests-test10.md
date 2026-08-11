# test10

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** giri   **Criterion:** (none — default criterion)
- **Diff:** 1 line missing (line 13) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees, leaving the frontier map empty for `main` and causing line 13's control-dep branch to be excluded from the slice.

## What the test does
`str.c` accepts a string argument, copies it, sorts its characters with `qsort`, prints the sorted string, and returns the first character of the original string. Line 13 contains `if (argc < 2)` — a guard that prints usage to stderr and calls `exit(EXIT_FAILURE)` if no argument is provided. The criterion (no explicit criterion file) defaults to the return instruction of `main` at line 28 (`return str[0];`). The slice should include line 13 because the branch at line 13 controls whether execution reaches the sorting and return path.

## Stage-by-stage output

### Stage 1 — make clean
Silent (expected).

### Stage 2 — clang -emit-llvm str.c → str.bc
Silent (expected).

### Stage 3 — llvm-link str.bc → str.all.bc
Silent (expected).

### Stage 4 — opt -trace-giri str.all.bc → str.trace.bc
Silent on both stdout and stderr (expected).

### Stage 5 — llc str.trace.bc → str.trace.s
Silent (expected).

### Stage 6 — clang++ str.trace.s → str.trace.exe
Silent (expected).

### Stage 7 — ./str.trace.exe giri
- stdout: `riig` (sorted characters of "giri")
- stderr: silent
- exit status: 103 ('g' ASCII value, correct — `return str[0]` returns the first character)

### Stage 8 — opt -dgiri str.all.bc → str.slice
- stdout: silent
- stderr: `Could not find Control-dep of this Basic Block` (emitted from `lib/Giri/Giri.cpp:153`)

### Stage 9 — sed/awk/sort/uniq → str.slice.loc
Output (3 lines): 18 19 28. No errors.

### Stage 10 — diff str.slice.loc ans-inst.txt
Diff output:
```
0a1
> 13
EXIT:1
```
Line 13 is in the golden file but absent from the computed slice.

## Diff against golden
Golden (4 lines): 13 18 19 28
Computed (3 lines): 18 19 28

Missing line: 13 (`if (argc < 2) {` in `main`).

This instruction is in the entry block of `main` and its branch controls whether execution reaches the blocks containing lines 18, 19, and 28 (the malloc, strcpy, and return path). It should appear as a control-dependent instruction in the backward slice. It is absent because `findExecForcers` returned an empty set for blocks in `main`, causing `Trace->getExecForcer` to return `nullptr` (`Giri.cpp:150`), which triggered the `"Could not find Control-dep of this Basic Block"` warning (`Giri.cpp:153`) and skipped including the branch condition.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) contains `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. In contrast, the original 3.4 code (on `master`) has `if (getRoots().empty()) return S;` followed by `if (BB) { …pred loop… }` — the null-block check guards only the predecessor loop, and the recursive call `calculate(DT, IDominee)` (line 48) runs for all children regardless.
   - **Why it matters here:** `main` has two exits: one path calls `exit()` (line 15), the other path `ret`s (line 28). LLVM 5.0.2's `PostDominatorTree` creates a *virtual root* node whose `getBlock()` returns `nullptr` for such functions. The port's early return at line 37 exits `calculate` before recursing into children, leaving the `Frontiers` map empty for the entire `main` function.
   - **Chain to the symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set for every block in `main` → `findExecForcers` (`Giri.cpp:86-98`) populates nothing in `ForceExecCache` → `getExecForcer` returns `nullptr` → warning at `Giri.cpp:153` → branch at line 13 never added to worklist → line 13 absent from slice output.
   - **Confirmed by:** `git show master:lib/Utility/PostDominatorFrontier.cpp` preserves the recursive child walk for null-BB nodes; the port does not. Same root cause as test3 and test5.

2. **stderr warning from `lib/Giri/Giri.cpp:153`.**
   - **Verdict:** FAIL-BUG (symptom of root cause 1).
   - **Evidence:** `errs() << "Could not find Control-dep of this Basic Block \n"` — emitted during the `-dgiri` pass. The message is the direct consequence of the empty frontier map described in root cause 1.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard that only wraps the predecessor loop (lines 39-44), mirroring the original 3.4 structure. This allows the child recursion at lines 46-55 to run even when the current node is the virtual root with a null block. Identified in sibling audits (test3, test5).