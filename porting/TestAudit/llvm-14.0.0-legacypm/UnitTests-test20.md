# test20 (UnitTests/test20)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (11 lines)   **Input:** (see per-test `INPUT` in `test/UnitTests/test20/Makefile`)
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** -1 (default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/UnitTests/test20/README.md` (3.4-era, unchanged). Program (`even.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/test20.log`, commit 74b870f):

```
Program stdout (traced run):
  even.c:8:16: warning: implicit declaration of function 'is_odd' is invalid in C99 [-Wimplicit-function-declaration]
          return is_odd(n - 1);
                 ^
  1 warning generated.
```