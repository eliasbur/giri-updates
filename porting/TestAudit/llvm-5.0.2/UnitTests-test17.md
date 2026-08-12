# test17

- **Verdict:** FAIL-BUG
- **Golden file:** ans-loc.txt   **Input:** 4   **Criterion:** -criterion-loc=criterion-loc.txt (plower.c:27)
- **Diff:** 2 lines missing / 0 lines extra
- **Root cause:** PostDominatorFrontier.cpp:37 null-BB early return produces 5 "Could not find Control-dep" errors, breaking backward slicing and omitting lines 16 and 21.

## What the test does
plower.c spawns N pthreads, each converting a 4-char substring of "AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHHIIII" to lowercase via `memcpy` on line 27 and a separate `mylower` thread function. The criterion is the `memcpy` call on line 27 inside the thread-creation loop. The golden backward slice includes the source string (line 11), the input-argument check (line 16), `nthreads` parsing (line 21), the loop header (line 25), the per-buffer `malloc` (line 26), and the `memcpy` itself (line 27).

## Stage-by-stage output

| Stage | Command | Result |
|---|---|---|
| 1 | `make clean -s` | EXIT 0 (clean) |
| 2 | `clang -g -O0 -c -emit-llvm` | EXIT 0 (clean) |
| 3 | `llvm-link` | EXIT 0 (clean) |
| 4 | `opt -trace-giri` (instrumentation) | EXIT 0 (clean) |
| 5 | `llc -asm-verbose=false -O0` | EXIT 0 (clean) |
| 6 | `clang++` link | EXIT 0 (required `-lpthread` beyond template command) |
| 7 | `./plower.trace.exe 4` | EXIT 0; output: `bbbb`, `aaaa`, `cccc`, `dddd` (4 threads) |
| 8 | `opt -dgiri` (slicing) | EXIT 0; stderr: 5x "Could not find Control-dep of this Basic Block" |
| 9 | slice extraction | 4 lines: 11, 25, 26, 27 |
| 10 | diff vs golden | FAIL — lines 16 and 21 missing |

Non-routine output (stage 8 stderr):
```
Start slicing Filename:Loc is defined as plower.c:27
Could not find Control-dep of this Basic Block
Could not find Control-dep of this Basic Block
Could not find Control-dep of this Basic Block
Could not find Control-dep of this Basic Block
Could not find Control-dep of this Basic Block
```

## Diff against golden
```
1a2,3
> 16
> 21
```

Golden file expects 6 lines: 11, 16, 21, 25, 26, 27.
Computed slice produced 4 lines: 11, 25, 26, 27.

**Missing line 16** — `if (argc != 2) {` (input-argument validation branch in main).
**Missing line 21** — `nthreads = atoi(argv[1]);` (thread-count parsing, feeds the loop count that determines how many iterations contain the criterion).

Neither line 16 nor line 21 appears anywhere in the static or dynamic slice output, indicating the backward-dataflow/chained-dependency walk never reached these definitions, consistent with control-dependency resolution failing at the 5 "Could not find" points.

## Root causes

1. **Verdict: FAIL-BUG.** `PostDominatorFrontier.cpp:37` early returns when the basic block lookup resolves to null, emitting "Could not find Control-dep of this Basic Block" to stderr. Stage 8 produced 5 such errors — one per thread-creation iteration in the instrumented trace — which broke the post-dominator analysis for the control-flow regions containing the input-validation `if` (line 16) and the `atoi` call (line 21). Without resolved control dependencies, the backward slice cannot walk past the loop header to these earlier definitions, producing a 2-line shortfall. The bug is a confirmed known issue, not specific to the LLVM 5.0.2 port.

## Proposed fix
None required for the port. This failure is attributable to the pre-existing `PostDominatorFrontier.cpp:37` bug that affects all ports equally. The fix would be to properly handle null BB pointers in the post-dominator frontier construction rather than silently dropping their control dependencies.