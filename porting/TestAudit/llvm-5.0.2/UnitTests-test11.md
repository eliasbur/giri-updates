# test11

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 4   **Criterion:** (none — default criterion, return of main)
- **Diff:** 4 lines missing (lines 23, 28, 29, 41) / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return `if (!BB) return S;` skips child recursion for virtual-root PostDominatorTrees in `main` (which has 3 exit points), emptying the frontier map and causing 5 "Could not find Control-dep" warnings from `Giri.cpp:153`, excluding 4 lines from the slice.

## What the test does
`hello2p.c` is a multi-threaded program from CS:APP chapter 12.4. `main` parses a thread count argument, creates N pthreads that each compute the length of a string from a shared array, joins them in a loop accumulating `total_len`, and returns it. The program uses `pthread`, `malloc`/`free`, and has a known data race (reuses the same `tid` variable in the creation loop). The criterion defaults to the return instruction of `main` at line 47 (`return total_len;`). The slice should include setup code (argc/nthreads guards) and the thread-creation loop because all of those are control-dependently required for `total_len` to reach its final value.

## Stage-by-stage output

### Stage 1 — make clean
Silent (expected).

### Stage 2 — clang -emit-llvm hello2p.c → hello2p.bc
Silent (expected).

### Stage 3 — llvm-link hello2p.bc → hello2p.all.bc
Silent (expected).

### Stage 4 — opt -trace-giri hello2p.all.bc → hello2p.trace.bc
Silent on both stdout and stderr (expected).

### Stage 5 — llc hello2p.trace.bc → hello2p.trace.s
Silent (expected).

### Stage 6 — clang++ hello2p.trace.s → hello2p.trace.exe
Silent (expected).

### Stage 7 — ./hello2p.trace.exe 4
- stdout: silent
- stderr: silent
- exit status: 52 (sum of truncated string lengths from 4 pthread joins; affected by data race bug)
- `hello2p.trace` file created (6944 bytes)

### Stage 8 — opt -dgiri hello2p.all.bc → hello2p.slice
- stdout: `Could not find Control-dep of this Basic Block` (5 occurrences)
- stderr: silent
- The 5 warnings come from `lib/Giri/Giri.cpp:153`, emitted each time `Trace->getExecForcer()` returns `nullptr` for a dynamic BB in `main` that requires a control-dependency forcer.

### Stage 9 — sed/awk/sort/uniq → hello2p.slice.loc
Output (3 lines): 21, 44, 47. No errors.

### Stage 10 — diff hello2p.slice.loc ans-inst.txt
Diff output:
```
1a2,5
> 23
> 28
> 29
> 41
```
EXIT: 1 (files differ)

## Diff against golden
Golden (7 lines): 21 23 28 29 41 44 47
Computed (3 lines): 21 44 47

Missing lines:
- **Line 23:** `if (argc < 2) {` — first argument-guard branch. Its control dependency is needed because the `exit(EXIT_FAILURE)` at line 25 is one of three exit paths. Resolving this branch requires post-dominator frontiers, which are empty due to the bug.
- **Line 28:** `nthreads = atoi(argv[1]);` — data value that flows to loop bounds (`for (i = 0; i < nthreads; i++)`) and indirectly to the thread-creation calls. Missing because the control-dep chain from the guarding `if (argc < 2)` at line 23 broke.
- **Line 29:** `if (nthreads > 4) {` — second guard branch. Like line 23, its control dependency on the PDOM frontier cannot be resolved.
- **Line 41:** `for (i = 0; i < nthreads; i++) {` — the thread-creation loop. Missing because its control dependency on the preceding branches failed to resolve.

## Root causes

1. **`PostDominatorFrontier.cpp:37` — early return for virtual-root node skips child recursion.**
   - **Verdict:** FAIL-BUG
   - **Evidence:** The port's `calculate()` function (line 37) contains `if (!BB) return S;`, which returns immediately when `Node->getBlock()` is `nullptr`. The original 3.4 code on `master` guards only the predecessor loop with the null-block check, and the recursive call `calculate(DT, IDominee)` (line 48) runs for all children regardless.
   - **Why it matters here:** `main` has **three** exits: `exit(EXIT_FAILURE)` at line 25, `exit(EXIT_FAILURE)` at line 31, and `return total_len` at line 47. LLVM 5.0.2's `PostDominatorTree` creates a *virtual root* node whose `getBlock()` returns `nullptr` for such functions. The port's early return at line 37 exits `calculate` before recursing into children, leaving the `Frontiers` map empty for all blocks in `main`.
   - **Chain to the symptom:** Empty frontiers → `PDF.getFrontier()` returns empty set for every block in `main` → `findExecForcers` (`Giri.cpp:86-98`) populates nothing in `ForceExecCache` → `getExecForcer` returns `nullptr` → warning at `Giri.cpp:153` × 5 → control-dep branching instructions at lines 23, 29 and the for-loop header at line 41 never added to worklist → their data-dependent successors at line 28 also missing → only lines 21, 44, 47 appear in the computed slice.
   - **Confirmed by:** `git show master:lib/Utility/PostDominatorFrontier.cpp` preserves the recursive child walk for null-BB nodes; the port does not. Same root cause confirmed in sibling audits (test3, test5, test8, test9, test10).

2. **stderr warnings from `lib/Giri/Giri.cpp:153`.**
   - **Verdict:** FAIL-BUG (symptom of root cause 1).
   - **Evidence:** 5 occurrences of `errs() << "Could not find Control-dep of this Basic Block \n"` emitted during the `-dgiri` pass. The message is the direct consequence of the empty frontier map described in root cause 1.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: Replace `if (!BB) return S;` with an `if (BB) { … }` guard that only wraps the predecessor loop (lines 39-44), mirroring the original 3.4 structure. This allows the child recursion at lines 46-55 to run even when the current node is the virtual root with a null block. Identified in sibling audits (test3, test5, test8, test9, test10).