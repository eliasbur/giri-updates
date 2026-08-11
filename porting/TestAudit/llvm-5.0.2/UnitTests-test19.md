# test19

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 7   **Criterion:** (none, defaults to exit status)
- **Diff:** empty
- **Root cause:** none

## What the test does
Source: `fibonacci.c` — implements a direct recursive fibonacci function `fibocci(int n)` with base cases `n < 0 → 0`, `n == 0 || n == 1 → n`, and recursive step `fibocci(n-1) + fibocci(n-2)`. `main()` reads `n` from `argv[1]` via `atoi`, calls `fibocci(n)`, returns `ret % 32`. With input 7, `fib(7) = 13`, so exit code is 13. Criterion is the program's exit status (no explicit criterion-loc).

## Stage-by-stage output
| Stage | Exit | Notes |
|---|---|---|
| 1. make clean | 0 | clean |
| 2. clang -emit-llvm | 0 | clean |
| 3. llvm-link | 0 | clean |
| 4. opt -trace-giri (instrument) | 0 | clean |
| 5. llc | 0 | clean |
| 6. clang++ (link) | 0 | clean |
| 7. ./fibocci.trace.exe 7 | 13 | correct exit code (fib(7)=13, 13%32=13) |
| 8. opt -dgiri (slice) | 0 | clean, no "Could not find Control-dep" or other warnings |
| 9. sed/awk/sort/uniq | 0 | clean |
| 10. diff vs golden | 0 | empty diff |

## Diff against golden
(empty)

## Root causes
No issues. Test passes cleanly on LLVM 5.0.2. The trace file (19072 bytes) was generated correctly, the slicer produced `fibocci.slice` without errors, and `fibocci.slice.loc` matches `ans-inst.txt` exactly (10 lines: 6, 8, 9, 11, 12, 19, 24, 25, 28, 29).

## Proposed fix
none