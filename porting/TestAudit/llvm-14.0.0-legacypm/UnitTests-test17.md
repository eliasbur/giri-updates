# test17 (UnitTests/test17)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt / ans-loc.txt (6 lines each)   **Input:** (see per-test `INPUT` in `test/UnitTests/test17/Makefile`)
- **Criterion:** criterion-inst.txt (`main 56`) + criterion-loc.txt (`plower.c 27`)
- **EXPECTED_EXIT:** -1 (default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/UnitTests/test17/README.md` (3.4-era, unchanged). Program (`plower.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/test17.log`, commit 74b870f):

```
Program stdout (traced run):
  aaaa
  bbbb
  cccc
  dddd
Slice-stage progress:
  Start slicing Filename:Loc is defined as plower.c:27
```