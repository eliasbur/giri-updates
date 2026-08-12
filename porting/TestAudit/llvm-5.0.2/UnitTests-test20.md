# test20

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 719   **Criterion:** (none — default from trace)
- **Diff:** empty
- **Root cause:** N/A — test passes without issues

## What the test does
Tests indirect mutual recursion: `is_even(n)` delegates to `is_odd(n-1)` and vice versa, descending to base case 0. Input 719 (odd) causes 719 recursive alternations through both functions. The criterion is the program's return value (`is_even(719)` → 0), expected to trace back through all lines in both recursive functions plus the argument parsing in `main`.

## Stage-by-stage output
1. **make clean -s** — exit 0, no output
2. **clang compile** — exit 0, stderr: `even.c:8:16: warning: implicit declaration of function 'is_odd' is invalid in C99` (expected for mutual recursion without a forward declaration)
3. **llvm-link** — exit 0, no output
4. **opt (trace-giri instrumentation)** — exit 0, no output
5. **llc (bitcode to ASM)** — exit 0, no output
6. **clang++ (link with librtgiri)** — exit 0, no output
7. **./even.trace.exe 719** — exit 0 (719 is odd, `is_even(719)` returns 0)
8. **opt (dgiri slicing)** — exit 0, no stderr output. No "Could not find Control-dep" warning (KNOWN BUG not triggered here).
9. **sed/awk/sort/uniq (extract slice lines)** — produces `even.slice.loc` with 11 lines, matching golden exactly
10. **diff** — exit 0, no differences

## Diff against golden
No diff — `even.slice.loc` matches `ans-inst.txt` exactly (11 lines: 5, 8, 9, 12, 13, 15, 16, 22, 27, 29, 30).

## Root causes
None. The test passes cleanly with output identical to the LLVM 5.0.2 port.

## Proposed fix
None required.