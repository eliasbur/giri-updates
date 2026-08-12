# test13

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 10   **Criterion:** (none — CRITERION empty, defaults to last instruction)
- **Diff:** empty (0 lines missing / 0 lines extra)
- **Root cause:** none; test passes cleanly with no warnings or errors

## What the test does
`struct.c` tests slicing of a program that uses C structs and nested conditionals. `main` reads an integer argument `x` (10), then branches on three paths: `x < 0` (lines 18-23), `x == 0` (lines 25-30), and `x > 0` (lines 31-35). Each branch populates two `result_t` structs using `sqrt()` and `pow()` calls. After the if/else chain converges, the program prints `y.result` and `z.result` via `printf` and returns the character count from the first `printf`. With input 10, execution follows the `x > 0` branch (line 31: `sqrt(10 * 3) = sqrt(30) = 5`, line 33: `pow(4, 10) = 1048576`), printing `5` and `1048576`. The function has a single return statement at line 41, so all control flow converges to one exit point.

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm struct.c -o struct.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link struct.bc -o struct.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... struct.all.bc -o struct.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 struct.trace.bc -o struct.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing struct.trace.s -o struct.trace.exe -L/giri/build/lib -lrtgiri -lm`
No output (stdout or stderr).

### Stage 7: `./struct.trace.exe 10`
- stdout: `5\n1048576\n`
- stderr: empty
- Program exits normally; trace file `struct.trace` generated successfully.

### Stage 8: `opt ... -dgiri ... struct.all.bc -o /dev/null`
- stdout: empty
- stderr: empty
- No warnings at all: zero "Could not find Control-dep", zero "failed DV/BLOCK", zero "Return and BB record doesn't match", zero "exceeds maximum".

### Stage 9: `sed ... struct.slice | awk ... | sort -g | uniq > struct.slice.loc`
No errors. Generated `struct.slice.loc` with 6 lines: `15 17 24 31 38 41`.

### Stage 10: `diff struct.slice.loc ans-inst.txt`
Exit code 0. Empty diff output.

## Diff against golden

No differences. Golden and computed slice are identical:
- Lines in both: 15, 17, 24, 31, 38, 41

These correspond to:
- **15**: `x = atoi(argv[1])` — input reading (data dependency for all branches)
- **17**: `if (x < 0)` — first branch condition (control dependency for branch selection)
- **24**: `if (x == 0)` — second branch condition (nested conditional)
- **31**: `y.result = sqrt(x * 3)` — taken branch body (actual computation path for x=10)
- **38**: `ret = printf("%d\n", y.result)` — output (criterion-adjacent)
- **41**: `return ret` — criterion instruction (last instruction in function)

## Root causes

None. This test passes cleanly. Unlike tests 3, 8, 9, 12, and 14, `struct.c`'s `main` function has a **single exit point** (`return ret` at line 41). LLVM 5.0.2's `PostDominatorTree` can compute a real (non-virtual) root for such functions, so `PostDominatorFrontier.cpp:37`'s early return `if (!BB) return S;` does not trigger. The frontier map is correctly populated, `getExecForcer` resolves all control dependencies, and the slice output matches the golden file exactly. The nested if/else chain exercises conditional control flow without exposing multi-exit frontier computation issues.

## Proposed fix

none