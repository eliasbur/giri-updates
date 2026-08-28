# even (test/UnitTests/test20)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (11 lines)   **Input:** 719
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** -1 (harness default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0; independently re-verified by the standalone-`tracer` validation: 11 slice locs == golden, prtrace OK)
- **Root cause:** none — test passes cleanly on LLVM 15.0.0 (new pass manager)

## What the test does
Program (`even`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 15.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff 72258e4..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files `test/Makefile.common`
and `test/HelloWorld/Makefile` changed, for the 15.0.0 toolchain parity). The 15.0.0
harness carries `-no-pie` (link), `-Wno-error=implicit-function-declaration` and
`-Xclang -no-opaque-pointers` (compile) — see SUMMARY.md; the numbered IR is
byte-compatible with the 3.4 reference because of the typed-pointer emit.

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/UnitTests_test20.log`: (the captured `warning:` line is the Clang 15 compile-time stderr for `even.c`,
  downgraded from an error by the harness's `-Wno-error=implicit-function-declaration` —
  not program stdout; see SUMMARY.md.)

```
  even.c:8:16: warning: call to undeclared function 'is_odd'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
          return is_odd(n - 1);
                 ^
  1 warning generated.
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); no `Start slicing` line appears in this log (the criterion is implicit); and no `[GIRI] Abnormal termination` line appears in the captured log.
