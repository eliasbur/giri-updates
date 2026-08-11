# test18

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** Mingliang LIU   **Criterion:** (none; derived from trace)
- **Diff:** empty
- **Root cause:** N/A — test passes cleanly

## What the test does
Source: `extlibcalls.c` — tests slicing through external library calls (malloc, memset, strcpy, strcat, strlen, printf) with a global variable `input` of type `char *`. `main()` allocates a 128-byte buffer, copies `argv[1]` into it via `strcpy`, appends `argv[2]` via `strcat`, then prints the concatenated string and its length, returning the length. The slicing criterion is the dynamic exit point (`return strlen(input)`) inferred from the trace file (no `-criterion-loc` flag in the Makefile).

## Stage-by-stage output
| Stage | Command | Exit | Notes |
|---|---|---|---|
| 1 | `make clean -s` | 0 | No output |
| 2 | `clang -g -O0 -c -emit-llvm extlibcalls.c -o extlibcalls.bc` | 0 | No output |
| 3 | `llvm-link extlibcalls.bc -o extlibcalls.all.bc` | 0 | No output |
| 4 | `opt -trace-giri … -o extlibcalls.trace.bc` | 0 | No warnings |
| 5 | `llc -asm-verbose=false -O0 extlibcalls.trace.bc -o extlibcalls.trace.s` | 0 | No output |
| 6 | `clang++ … -o extlibcalls.trace.exe -lrtgiri` | 0 | No output |
| 7 | `./extlibcalls.trace.exe Mingliang LIU` | 12 | stdout: "MingliangLIU\n12" (strlen=12, correct return) |
| 8 | `opt -dgiri … extlibcalls.all.bc -o /dev/null` | 0 | No stderr; no "Could not find Control-dep" warnings |
| 9 | `sed/awk/sort/uniq > extlibcalls.slice.loc` | 0 | Produced lines: 11, 14, 15, 20 |
| 10 | `diff extlibcalls.slice.loc ans-inst.txt` | 0 | Empty diff |

## Diff against golden
Empty — exact match.

## Root causes
N/A. All stages completed without errors or warnings. The PostDominatorFrontier early-return bug (stage 8 "Could not find Control-dep") did not manifest for this test.

## Proposed fix
None — test is CLEAN.