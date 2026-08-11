# hellothreads (test5)

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 8   **Criterion:** (none, defaults to final `ret` of `main`)
- **Diff:** 12 lines missing / 0 lines extra
- **Root cause:** `PostDominatorFrontier.cpp:37` early return at virtual root empties the post-dominator frontier for functions with multiple exits, causing `Could not find Control-dep` failures that truncate the backward slice to only the criterion's immediate basic block.

## What the test does
`hellothreads.c` creates a user-specified number of pthreads (INPUT=8), each calling `PrintHello` which prints a numbered hello message via `printf`. `main` allocates thread/id arrays with `calloc`, calls `pthread_create` in a loop, then `pthread_join` in a second loop, and returns 0. `handle_error_en` is a macro wrapping `perror` + `exit`. The slicing criterion defaults to the final `ret i32` of `main` (line 46). The golden answer expects 12 lines: 23 (num_threads), 28-29 (calloc), 30-32-34-35 (create loop body + body of handle_error_en check), 39-40-41 (join loop), 45-46 (return).

## Stage-by-stage output

### Stage 1 — Clean
No output on stdout or stderr.

### Stage 2 — Compile (clang -> hellothreads.bc)
No output on stdout or stderr.

### Stage 3 — Link (llvm-link -> hellothreads.all.bc)
No output on stdout or stderr.

### Stage 4 — Instrument (opt -trace-giri -> hellothreads.trace.bc)
No output on stdout or stderr.

### Stage 5 — LLC (llc -> hellothreads.trace.s)
No output on stdout or stderr.

### Stage 6 — Compile (clang++ -> hellothreads.trace.exe)
No output on stdout or stderr.

### Stage 7 — Run (./hellothreads.trace.exe 8)
**stdout:** 8 lines, one per thread, e.g. `[0] Hello thread 139790780585728!` through `[7] Hello thread 139790649390848!`
**stderr:** empty.
**Exit code:** 0.

### Stage 8 — Slice (opt -dgiri -> hellothreads.slice)
**stdout:** empty.
**stderr (verbatim):**
```
Could not find Control-dep of this Basic Block 
Could not find Control-dep of this Basic Block 
```
Two instances, emitted from `lib/Giri/Giri.cpp:153`. These indicate that during the construction of the dynamic control dependence graph, the slicer could not find a post-dominator (control dependency target) for two basic blocks. This occurs because `PostDominatorFrontier.cpp:37` has `if (!BB) return S;` which returns early at the virtual entry node, skipping child recursion and leaving the frontier map empty for functions with multiple exit points.

### Stage 9 — Extract locations (sed/awk/sort/uniq -> hellothreads.slice.loc)
**stdout:** only `45` and `46` (two lines).
**stderr:** empty.

The slice file confirms the slice contains only the `ret i32 %90` instruction at the end of `main` and its immediate predecessors (the alloca and the store to the return value). All data/control dependencies on `pthread_create`, `pthread_join`, `calloc`, `num_threads`, and the entire `PrintHello` function are missing.

### Stage 10 — Diff (diff hellothreads.slice.loc vs ans-inst.txt)
**stdout (verbatim):**
```
0a1,12
> 23
> 28
> 29
> 30
> 32
> 34
> 35
> 39
> 40
> 41
> 45
> 46
```
**Exit code:** 1 (diff found differences).
All 12 expected lines are missing from the generated output.

## Diff against golden

| Missing line | Purpose in source | Why it's missing |
|---|---|---|
| 23 | `int num_threads = atoi(argv[1])` | Data dep: thread count from argument |
| 28 | `threads = (pthread_t *)calloc(...)` | Data dep: thread ID array allocation |
| 29 | `myid = (long *)calloc(...)` | Data dep: thread index array allocation |
| 30 | `for(t = 0; t < num_threads; t++)` | Control dep: create loop |
| 32 | `rc = pthread_create(...)` | Data dep: thread creation |
| 34 | `myid[t] = t` | Data dep: thread ID assignment |
| 35 | `if (rc != 0)` | Control dep: error handling in create loop |
| 39 | `for(t = 0; t < num_threads; t++)` | Control dep: join loop |
| 40 | `rc = pthread_join(...)` | Data dep: thread joining |
| 41 | `if (rc != 0)` | Control dep: error handling in join loop |
| 45 | `store i32 0, i32* %3` | Data dep: setting return value to 0 |
| 46 | `ret i32 %90` | Criterion: final return of main |

Lines 45 and 46 appear in both. The remaining 10 lines are missing because the backward slice was truncated to only the criterion's immediate basic block.

## Root causes

1. **FAIL-BUG: PostDominatorFrontier early return truncates control dependence graph.**
   - `lib/Utility/PostDominatorFrontier.cpp:37`: `if (!BB) return S;` returns immediately when called on the virtual entry node, never recursing into child blocks. This leaves the post-dominator frontier map empty for functions whose CFG has multiple exits (such as `main`, which has `handle_error_en` callers that expand to `exit()`, a non-returning function).
   - Evidence: Two `Could not find Control-dep of this Basic Block` messages from `lib/Giri/Giri.cpp:153` during stage 8.
   - Effect: Without control dependencies, the backward slice algorithm cannot traverse from the criterion through the `pthread_create`/`pthread_join` call chains and their data dependencies. The resulting slice only contains the `ret` instruction's immediate predecessors (alloca + store).
   - This matches the same root cause identified in the test3 sibling audit.

## Proposed fix
`lib/Utility/PostDominatorFrontier.cpp:37`: The early return `if (!BB) return S;` should continue recursing into `BB->succ_begin()` children even when `BB` is the null virtual root, so that the post-dominator frontier for real basic blocks is populated for functions with multiple exits. The fix is to remove or restructure this guard so it does not short-circuit the recursion for the virtual entry node. Same root cause as test3.