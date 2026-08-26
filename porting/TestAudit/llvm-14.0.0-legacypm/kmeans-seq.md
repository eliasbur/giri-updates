# kmeans-seq (kmeans)

- **Verdict:** CLEAN
- **Variant:** seq (Makefile yields to `TEST_PARALLELISM=seq` from the Dockerfile env)
- **Golden file:** ans-inst-seq.txt (2 lines)   **Input:** (see per-test `INPUT` in `test/kmeans/Makefile`)
- **Criterion:** criterion-inst-seq.txt (`main 120`)
- **EXPECTED_EXIT:** -1 (default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/kmeans/README.md` (3.4-era, unchanged). Program (`kmeans-seq.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/kmeans.log`, commit 74b870f):

```
Program stdout (traced run):
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
Slice-stage progress:
  Start slicing Function:Instruction is defined as main:120
```