# test15 (test/UnitTests/test15)

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt (12 lines)   **Input:** 7
- **Criterion:** none (slices at the implicit criterion)
- **EXPECTED_EXIT:** 31   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (new pass manager)

## What the test does
Program (`hanoi`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff fba2565..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files changed).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/UnitTests_test15.log`:

```
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

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slice criterion is printed by the slicing stage ("Start slicing
…") and no `[GIRI] Abnormal termination` line appears in the captured log.
