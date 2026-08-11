# test4

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** 10   **Criterion:** -criterion-loc=criterion-loc.txt
- **Diff:** empty
- **Root cause:** none — test passes correctly on LLVM 5.0.2

## What the test does
example.c is Example 1 from the _Dynamic Program Slicing_ paper. It reads an integer `x` from `argv[1]`, then branches: if `x < 0` it computes `y=sqrt(x)` and `z=pow(2,x)`; if `x == 0` it computes `y=sqrt(x*2)` and `z=pow(3,x)`; otherwise `y=sqrt(x*3)` and `z=pow(4,x)`. It prints `y` (line 26) and `z` (line 27), and returns `y`. The slicing criterion selects the two `printf` calls on lines 26 and 27 of `example.c`. With input 10, the program follows the else branch (x > 0, x ≠ 0), producing `y=5` (truncation of √30) and `z=1048576` (4¹⁰).

## Stage-by-stage output

### Stage 1: make clean
No output (stdout/stderr both empty).

### Stage 2: clang compile (example.c → example.bc)
No output.

### Stage 3: llvm-link (example.bc → example.all.bc)
No output.

### Stage 4: opt instrumentation (trace-giri)
No output.

### Stage 5: llc codegen (trace.bc → trace.s)
No output.

### Stage 6: clang++ link (trace.s → trace.exe)
No output.

### Stage 7: execution (./example.trace.exe 10)
stdout: `5`, `1048576`, `EXIT:5` — correct for input 10 (else-else branch: y=⌊√30⌋=5, z=4¹⁰=1048576, return code = y = 5).
stderr: empty.

### Stage 8: opt slicing (dgiri)
stdout: empty.
stderr: Two informational messages from the slicing pass:
```
Start slicing Filename:Loc is defined as example.c:26
Start slicing Filename:Loc is defined as example.c:27
```
These correspond to the two criterion locations from `criterion-loc.txt`. Routine algorithm progress logging.

### Stage 9: extract line numbers (slice → slice.loc)
No output. Generated `example.slice.loc`: `9 11 16 21 22 26 27`.

### Stage 10: diff (slice.loc vs ans-inst.txt)
Empty diff — generated output matches golden file exactly.

## Diff against golden
No differences. Generated line numbers (9, 11, 16, 21, 22, 26, 27) match the golden file verbatim.

## Root causes
None. The test produces the expected dynamic backward slice for both criterion points on LLVM 5.0.2. The two stderr lines from stage 8 are routine informational messages emitted once per criterion location by the `-dgiri` pass and do not indicate any issue.

## Proposed fix
none — test passes cleanly on LLVM 5.0.2