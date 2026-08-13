# pca-seq

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt   **Input:** (none)   **Criterion:** calc_mean 51
- **Diff:** empty
- **Root cause:** none — test passes cleanly on LLVM 5.0.2

## What the test does
`pca-seq.c` generates a 10×10 random matrix (values 0-100), centers each column by subtracting the mean, then computes the covariance matrix and prints it. The slicing criterion is `calc_mean:51` (the 51st LLVM instruction in the `calc_mean` function). The golden slice contains 17 source lines.

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm pca-seq.c -o pca-seq.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link pca-seq.bc -o pca-seq.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... pca-seq.all.bc -o pca-seq.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 pca-seq.trace.bc -o pca-seq.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing pca-seq.trace.s -o pca-seq.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./pca-seq.trace.exe`
Stdout: 10×10 random matrix, centered values (10 rows of 10 ints each), 10×10 covariance matrix. Exit code 0.
Stderr: empty.

### Stage 8: `opt ... -dgiri ... pca-seq.all.bc -o /dev/null`
Stderr: `Start slicing Function:Instruction is defined as calc_mean:51`
No warnings or errors. 0 "Could not find Control-dep" messages.

### Stage 9: `sed ... pca-seq.slice | awk ... | sort -g | uniq > pca-seq.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff pca-seq.slice.loc ans-inst-seq.txt`
Empty diff — files are identical.

## Diff against golden
Empty — no differences.

## Root causes
None. The test passes cleanly on the LLVM 5.0.2 port. The criterion resolves correctly (instruction 51 of `calc_mean`), the trace captures all necessary BB entries, and no control-dependence warnings are emitted.

## Proposed fix
None required.