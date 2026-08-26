# matrix_multiply-seq (matrix_multiply)

- **Verdict:** CLEAN
- **Variant:** seq (Makefile yields to `TEST_PARALLELISM=seq` from the Dockerfile env)
- **Golden file:** ans-inst-seq.txt (19 lines)   **Input:** (see per-test `INPUT` in `test/matrix_multiply/Makefile`)
- **Criterion:** criterion-inst-seq.txt (`matrix_mult 291`)
- **EXPECTED_EXIT:** -1 (default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/matrix_multiply/README.md` (3.4-era, unchanged). Program (`matrix_multiply-seq.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/matrix_multiply.log`, commit 74b870f):

```
Program stdout (traced run):
  MatrixMult: Side of the matrix is 4 
  MatrixMult: Running...
  MatrixMult: Calling Serial Matrix Multiplication
  8  3  5  7  
  7  4  8  7  
  2  4  9  9  
  0  3  8  1  
  3  2  4  0  
  6  4  6  10  
  2  9  4  3  
  8  5  8  3  
  108  108  126  66  
  117  137  140  85  
  120  146  140  94  
  42  89  58  57  
Slice-stage progress:
  Start slicing Function:Instruction is defined as matrix_mult:291
```