# even (test/UnitTests/test20)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (11 lines)   **Input:** 719
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** <unchecked>   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to the pristine 3.4-era golden (the harness `diff` exits 0; independently re-verified by the standalone-`tracer` validation: 11 slice locs == golden, prtrace OK)
- **Root cause:** none — test passes cleanly on LLVM 16.0.0 (new pass manager)

## What the test does
Program (`even`) compiled with `clang -g -O0 -c -emit-llvm -Xclang -no-opaque-pointers`
(clang/LLVM 16.0.0), linked (`llvm-link`), instrumented with the `trace-giri` pass under
**new pass manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`, each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with `dgiri` in the same pipeline against the criterion; the
extracted `file:line` list is diffed against the pristine 3.4-era golden. For this port
`git diff 63b02e2..HEAD -- test/` is **empty** — no golden, criterion, or harness file
changed. The 16.0.0 harness carries `-no-pie` (link), `-Wno-error=implicit-function-declaration`,
and `-Xclang -no-opaque-pointers` (compile) inherited from the 15.0.0 harness; the
opaque-pointer flag is **load-bearing** on the 16.0.0 ubuntu-18.04 prebuilt (typed IR
`i32*`/`i32**` with it, opaque `ptr` without), which keeps the numbered IR byte-compatible
with the 3.4 reference.

## Stage-by-stage output (honest harness, captured `_test_logs/$(basename).log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/test20.log` (see the per-test log under `_test_logs/`).
Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); no `[GIRI] Abnormal termination` line appears in the captured log.
