# matrix_multiply (test/matrix_multiply)

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt (19 lines)   **Input:** 4
- **Criterion:** -criterion-inst=criterion-inst-seq.txt (CRITERION_TYPE=inst, TEST_PARALLELISM=seq)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (new pass manager)

## What the test does
Program (`matrix_multiply-seq`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff fba2565..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files changed).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/matrix_multiply.log`:

```
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
  Start slicing Function:Instruction is defined as matrix_mult:291
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slice criterion is printed by the slicing stage ("Start slicing
…") and no `[GIRI] Abnormal termination` line appears in the captured log.
