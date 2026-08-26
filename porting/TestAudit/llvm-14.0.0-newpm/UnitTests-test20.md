# test20 (test/UnitTests/test20)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (11 lines)   **Input:** 719
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (new pass manager)

## What the test does
Program (`even`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff fba2565..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files changed).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/UnitTests_test20.log`:

```
  even.c:8:16: warning: implicit declaration of function 'is_odd' is invalid in C99 [-Wimplicit-function-declaration]
          return is_odd(n - 1);
                 ^
  1 warning generated.
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slice criterion is printed by the slicing stage ("Start slicing
…") and no `[GIRI] Abnormal termination` line appears in the captured log.
