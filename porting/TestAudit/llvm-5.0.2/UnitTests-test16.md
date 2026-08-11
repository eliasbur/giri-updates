# test16

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst.txt   **Input:** 10   **Criterion:** (none)
- **Diff:** N/A — slicing crashed before producing output
- **Root cause:** `TraceFile::findNextNestedID` crashes because store ID 4 (instruction namespace) collides with BB ID 4 (basic block namespace), causing false nesting increment that prevents locating the entry BB.

## What the test does

Compiles two source files: `struct-ptr.c` (main) and `calc.c`/`calc.h` (library). `main()` calls `atoi()` then `calc(x, &y, &z)`, then prints `y.result` and `z.result`. `calc()` branches on `x` (negative, zero, positive) computing `sqrt(x*3)` and `pow(4,x)` for the positive path. With input 10, expected output is `5` (truncated sqrt(30)) and `1048576` (pow(4,10)). Golden answer slices lines 7, 11, 13, 14, 15, 18 (from struct-ptr.c) and 21 (from calc.c).

## Stage-by-stage output

| Stage | Command | Result |
|-------|---------|--------|
| 1 | `make clean -s` | OK (clean) |
| 2 | `clang ... struct-ptr.c calc.c` | OK (both compile) |
| 3 | `llvm-link` | OK |
| 4 | `opt ... -trace-giri` | OK |
| 5 | `llc ...` | OK |
| 6 | `clang++ ... -lrtgiri -lm` | OK |
| 7 | `./struct-ptr.trace.exe 10` | Exit code 2 (expected — `return ret` where ret = printf's return value 2). Output: `5\n1048576\n`. Trace file: 1568 bytes, 49 Entry structs. |
| 8 | `opt ... -dgiri` | **CRASH — LLVM ERROR** |
| 9 | `sed ...` | Not reached |
| 10 | `diff ...` | Not reached |

Stage 8 stderr:
```
start_index: 2 type: B id: 1 nestID: 4
LLVM ERROR: Did not find desired subsequent entry in trace!
```
(Crash at `lib/Giri/TraceFile.cpp:410`.)

## Diff against golden

No diff possible — the sliced .slice file was never produced due to the crash. ans-inst.txt contains:
```
7
11
13
14
15
18
21
```

## Root causes

1. **`findNextNestedID` namespace collision (`TraceFile.cpp:378-410`).** The function is called from `findAllStoresForLoad` (`TraceFile.cpp:579`) searching for the BB entry that contains store id=4. The `nestID` parameter is set to `trace[store_index].id` = 4 (the store's instrumented instruction number). The search loop increments `nesting` whenever it encounters a BB entry whose `id` matches `nestID`. In this trace, a BB in `calc` is numbered 4 — coinciding with the store's instruction number 4. When the search encounters `B id=4` at trace entry 18, nesting goes to 1. The actual entry BB (`B id=1`) appears at trace entry 47 (at the end, after all calls return), but by then nesting=1 so it is skipped. The search exhausts the trace and hits `report_fatal_error`.

   This is a fundamental design flaw: the code mixes IDs from two separate namespaces (basic block IDs assigned by `BasicBlockNumberPass`, instruction IDs assigned by `LoadStoreNumberPass`) without offsetting them, and uses instruction IDs for nesting control in a BB-level search. Whether the collision manifests depends on the exact count of BBs and load/store/call instructions in the IR — a detail that can change between LLVM versions due to different codegen or pass behavior. In LLVM 5.0.2, the 4th basic block happens to share its number with the 4th instrumented instruction, triggering the bug.

## Proposed fix

Offset the two ID namespaces so they never overlap (e.g., start instruction IDs at `MAX_BB_COUNT + 1`, or use a distinct tag bit). Alternatively, refactor `findNextNestedID` to not conflate BB IDs with instruction IDs in the nesting logic. This is a pre-existing bug in the original codebase that happens to surface under LLVM 5.0.2's IR layout.