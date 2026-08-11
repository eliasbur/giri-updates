# test14

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 10   **Criterion:** (none; defaults to last ret)
- **Diff:** empty
- **Root cause:** none

## What the test does
`struct-ptr.c` defines a `result_t` struct and a `calc()` function with three branches based on the sign of `x`: negative (`sqrt(x)`/`pow(2,x)`), zero (`sqrt(x*2)`/`pow(3,x)`), and positive (`sqrt(x*3)`/`pow(4,x)`). `main()` reads an integer argument, calls `calc()`, and prints `y.result` and `z.result`. With input 10, the positive branch is taken, yielding `y.result = 5` (truncated sqrt(30)) and `z.result = 1048576` (4^10).

## Stage-by-stage output
| Stage | Command | stdout | stderr |
|-------|---------|--------|--------|
| 1. Clean | `rm -f *.ll *.bc ...` | (empty) | (empty) |
| 2. Compile | `clang -g -O0 -c -emit-llvm struct-ptr.c -o struct-ptr.bc` | (empty) | (empty) |
| 3. Link | `llvm-link struct-ptr.bc -o struct-ptr.all.bc` | (empty) | (empty) |
| 4. Instrument | `opt ... -trace-giri ... struct-ptr.all.bc -o struct-ptr.trace.bc` | (empty) | (empty) |
| 5. Codegen | `llc -asm-verbose=false -O0 struct-ptr.trace.bc -o struct-ptr.trace.s` | (empty) | (empty) |
| 6. Link exe | `clang++ ... struct-ptr.trace.s -o struct-ptr.trace.exe -L/giri/build/lib -lrtgiri -lm` | (empty) | (empty) |
| 7. Run | `./struct-ptr.trace.exe 10` | `5\n1048576` | (empty) |
| 8. Slice | `opt ... -dgiri -trace-file=struct-ptr.trace -slice-file=struct-ptr.slice ...` | (empty) | (empty) |
| 9. Extract | `sed ... | awk ... | sort -g | uniq > struct-ptr.slice.loc` | (empty) | (empty) |
| 10. Diff | `diff struct-ptr.slice.loc ans-inst.txt` | (empty — lines match) | (empty) |

No diagnostic strings found in any stderr: no `Could not find Control-dep`, no `failed DV/BLOCK/to find`, no `Return and BB record doesn't match`, no `start_index:` dumps, no `Number of monitored program points exceeds maximum value`.

## Diff against golden
Empty. Sliced lines {13, 20, 27, 40, 42, 44, 47} match golden exactly.

## Root causes
None. The test passes silently with no stderr noise across all stages. While the `calc()` function has three divergent branches (a pattern that can expose the `PostDominatorFrontier.cpp:37` early-return bug), all paths converge at a single return point, and the dynamic backward slice correctly isolates only the taken branch (line 27, the `x > 0` path). No "Could not find Control-dep" warning was emitted.

## Proposed fix
none