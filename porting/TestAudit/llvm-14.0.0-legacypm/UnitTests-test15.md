# test15 (UnitTests/test15)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (12 lines)   **Input:** (see per-test `INPUT` in `test/UnitTests/test15/Makefile`)
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** 31   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/UnitTests/test15/README.md` (3.4-era, unchanged). Program (`hanoi.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/test15.log`, commit 74b870f):

```
Program stdout (traced run):
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 4 from A to B
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 3 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 5 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 3 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 4 from B to C
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 6 from A to B
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 3 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 4 from C to A
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 3 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 5 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 4 from A to B
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 3 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 7 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 3 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 4 from B to C
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 5 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 3 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 4 from C to A
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 3 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 6 from B to C
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 4 from A to B
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 3 from C to B
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 5 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
  Move 3 from B to A
  Move 1 from C to B
  Move 2 from C to A
  Move 1 from B to A
  Move 4 from B to C
  Move 1 from A to C
  Move 2 from A to B
  Move 1 from C to B
  Move 3 from A to C
  Move 1 from B to A
  Move 2 from B to C
  Move 1 from A to C
```