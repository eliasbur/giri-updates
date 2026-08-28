# psum (test/UnitTests/test12)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (20 lines)   **Input:** 16 8
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** 28   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0; independently re-verified by the standalone-`tracer` validation: 20 slice locs == golden, prtrace OK)
- **Root cause:** none — test passes cleanly on LLVM 15.0.0 (new pass manager)

## What the test does
Program (`psum`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 15.0.0),
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
`_test_logs/UnitTests_test12.log`:

```
  The supposed sum is: 32640
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  i8* %0
  For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later
  …
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); no `Start slicing` line appears in this log (the criterion is implicit); and no `[GIRI] Abnormal termination` line appears in the captured log.
