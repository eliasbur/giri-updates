# test15 (hanoi)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 7   **Criterion:** (default, no criterion.txt)
- **Diff:** empty
- **Root cause:** N/A — all 10 stages completed with zero errors or warnings.

## What the test does
Recursively implements the Tower of Hanoi algorithm. `hanoi.c` defines `move(n, from, to, via)` which moves `n` disks, printing each move and returning the count of moves performed. `main()` reads the tower size from argv[1] (here 7), invokes `move(7, 'A', 'C', 'B')`, and returns `ret % 32`. For n=7, there are 2^7 - 1 = 127 moves; 127 % 32 = 31 (exit code). This test is labeled "external library calls" — the `printf` on line 8 is an external call exercised within the recursion.

## Stage-by-stage output
| Stage | Command | Exit | Notes |
|---|---|---|---|
| 1 | `make clean -s` | 0 | Clean, no output |
| 2 | `clang -g -O0 -c -emit-llvm hanoi.c` | 0 | No warnings |
| 3 | `llvm-link hanoi.bc -o hanoi.all.bc` | 0 | Single module, no output |
| 4 | `opt -trace-giri ... hanoi.all.bc` | 0 | No warnings |
| 5 | `llc -asm-verbose=false -O0 hanoi.trace.bc` | 0 | No warnings |
| 6 | `clang++ ... hanoi.trace.s -o hanoi.trace.exe -lrtgiri` | 0 | No warnings |
| 7 | `./hanoi.trace.exe 7` | 31 | 127 move lines printed; 127 % 32 = 31 |
| 8 | `opt -dgiri ... hanoi.all.bc` | 0 | No "Could not find Control-dep" warnings; no errors on stderr |
| 9 | `sed \| awk \| sort \| uniq > hanoi.slice.loc` | 0 | 12 unique line numbers extracted |
| 10 | `diff hanoi.slice.loc ans-inst.txt` | 0 | Identical output |

## Diff against golden
Empty — `hanoi.slice.loc` matches `ans-inst.txt` exactly (12 lines: 6, 7, 8, 10, 11, 12, 13, 20, 25, 26, 28, 29).

## Root causes
None. The test passes cleanly with no warnings or errors at any stage.

## Proposed fix
None.