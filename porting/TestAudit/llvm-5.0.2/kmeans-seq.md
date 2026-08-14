# kmeans-seq

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt   **Input:** (none)   **Criterion:** main 120
- **Diff:** empty
- **Root cause:** none — test passes cleanly on LLVM 5.0.2

## What the test does
`kmeans-seq.c` generates 100 points in 3-dimensional space (range 0-10), runs a k-means clustering algorithm with 10 clusters for 10 iterations, prints final cluster means, and returns 0 (line 287). The slicing criterion is `main:120` (the 120th LLVM instruction in the `main` function). The golden slice contains 2 source lines.

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm kmeans-seq.c -o kmeans-seq.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link kmeans-seq.bc -o kmeans-seq.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... kmeans-seq.all.bc -o kmeans-seq.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 kmeans-seq.trace.bc -o kmeans-seq.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing kmeans-seq.trace.s -o kmeans-seq.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./kmeans-seq.trace.exe`
Stdout: dimension info, "Generating points", "Generating means", "Starting iterative algorithm" (+ period trail), "Final Means:" (10 rows of 3 ints), "Cleaning up". Exit code 0 (program returns 0 at line 287).
Stderr: empty.

### Stage 8: `opt ... -dgiri ... kmeans-seq.all.bc -o /dev/null`
Stderr: `Start slicing Function:Instruction is defined as main:120`
No warnings or errors. 0 "Could not find Control-dep" messages.

### Stage 9: `sed ... kmeans-seq.slice | awk ... | sort -g | uniq > kmeans-seq.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff kmeans-seq.slice.loc ans-inst-seq.txt`
Empty diff — files are identical.

## Diff against golden
Empty — no differences.

## Root causes
None. The test passes cleanly on the LLVM 5.0.2 port. The criterion resolves correctly (instruction 120 of `main`), the trace captures all necessary BB entries, and no control-dependence warnings are emitted.

## Proposed fix
None required.