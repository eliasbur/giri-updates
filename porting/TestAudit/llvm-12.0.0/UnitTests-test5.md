# hellothreads (test5)

- **Verdict:** CLEAN
- **Variant:** seq (`TEST_PARALLELISM=seq` via `Dockerfile` env)
- **Golden file:** ans-inst.txt (**12** source-line entries)   **Pristine:** yes (no `ans-*.txt` change; `git diff 224bdfb..HEAD -- test/` is empty)
- **Criterion:** none (CRITERION empty)
- **Diff:** empty — `$(NAME).slice.loc` identical to the 3.4-era golden

## What the test does
Program compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 12.0.0), instrumented
with the `-trace-giri` pass (both pipeline stages apply the identical
`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum` sequence,
`test/Makefile.common`), traced by the real program, then sliced with `-dgiri`
against the none criterion. The extracted `file:line` list
(`sed -n '/^Source.*[0-9]+$/p' | awk -F: '{print $3}' | sort -g | uniq`) is
diffed against the 3.4-era golden, which is pristine.

## Stage-by-stage output (honest harness, per-test `_test_logs/UnitTests_test5.log`)
Stages 1–6 (clean, `clang`, `llvm-link`, `opt -trace-giri`, `llc`, `clang++` link
against `librtgiri.a`): silent, no warnings (test20 carries a pre-existing,
unchanged `implicit declaration` note; no `-Wno-error` is applied on 12.0.0 and
it compiles cleanly).

Stage 7 (run traced binary):
```
 ===
[1] Hello thread 139844356429568!
[0] Hello thread 139844364822272!
[2] Hello thread 139844348036864!
[3] Hello thread 139844269373184!
[4] Hello thread 139844260980480!
[5] Hello thread 139844252587776!
[6] Hello thread 139844244195072!
[7] Hello thread 139844235802368!
=== 
```

No `[GIRI] Abnormal termination` marker. pthread test (LDFLAGS=-pthread); both threads print; no `[GIRI] Abnormal` marker.

Stage 8 (`opt -dgiri` slice): routine `Start slicing …` progress message(s) only.
The 12.0.0 prebuilt `opt` prints nothing for `-stats` (Release build without
stats; stderr-only, never pollutes the `.ll`/trace/slice files — same benign
`-stats` suppression noted on 8.0.0/14.0.0).

Stage 10 (`diff $(NAME).slice.loc $(TEST_ANS)`): empty diff — files identical.

## Diff against golden
Empty — no differences. The golden is byte-identical to the pristine 3.4 base;
no regeneration was needed or performed, and no `test/` file was changed by this
port (`git diff 224bdfb..HEAD -- test/` is empty).

## Root causes
None. The test passes cleanly on the LLVM 12.0.0 port.

## Proposed fix
None required.
