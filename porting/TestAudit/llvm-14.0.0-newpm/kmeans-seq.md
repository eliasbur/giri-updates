# kmeans (test/kmeans)

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt (2 lines)   **Input:** 16
- **Criterion:** -criterion-inst=criterion-inst-seq.txt (CRITERION_TYPE=inst, TEST_PARALLELISM=seq)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (new pass manager)

## What the test does
Program (`kmeans-seq`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff fba2565..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files changed).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/kmeans.log`:

```
  Dimension = 3
  Number of clusters = 10
  Number of points = 100
  Size of each dimension = 10
  Generating points
  Generating means
  Starting iterative algorithm
  ..........
  Final Means:
      3     1     3 
      1     5     8 
      6     7     6 
      5     7     1 
      6     2     6 
      7     3     1 
      1     7     3 
      0     3     0 
      2     0     7 
      5     1     0 
  Cleaning up
  Start slicing Function:Instruction is defined as main:120
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slice criterion is printed by the slicing stage ("Start slicing
…") and no `[GIRI] Abnormal termination` line appears in the captured log.
