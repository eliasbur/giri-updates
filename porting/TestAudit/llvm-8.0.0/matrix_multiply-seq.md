# matrix_multiply-seq (matrix_multiply)

- **Verdict:** CLEAN
- **Variant:** seq (`TEST_PARALLELISM=seq` via `Dockerfile` env, `Makefile` yields to it)
- **Golden file:** ans-inst-seq.txt   **Input:** (see per-test `INPUT` in `test/matrix_multiply/Makefile`)
- **Criterion:** criterion-inst-seq.txt (`matrix_mult 291`)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (19 lines: 56, 90, 97, 113, 119, 120, 121, 122, 123, 129, 131, 133, 137, 139, 141, 145, 147, 149, 154)
- **Root cause:** none — test passes cleanly on LLVM 8.0.0

## What the test does
See `test/matrix_multiply/README.md` (3.4-era, unchanged). Program compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 8.0.0), instrumented with the
`-trace-giri` pass, traced, then sliced with `-dgiri` against the criterion;
the extracted `file:line` list is diffed against the 3.4-era golden, which is
pristine (verified `git diff 86f3b8a..HEAD` shows no `ans-*.txt` changes).

## Stage-by-stage output (honest harness, per-test `_test_logs/*.log`)
Stages 1–6 (clean, clang, llvm-link, opt `-trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings.

Stage 7 (run traced binary): program output as expected; exit code matches
`EXPECTED_EXIT=-1` (or unchecked, `EXIT_UNCHECKED=0`); no
`[GIRI] Abnormal termination` marker.

Stage 8 (opt `-dgiri` slice): routine per-criterion `Start slicing ...`
progress messages only. LLVM 8's release toolchain prints
`Statistics are disabled. Build with asserts or with -DLLVM_ENABLE_STATS`
where `-stats` is requested (Release build without stats) — benign, same as
5.0.2's `-stats` suppression note.

Stage 10 (`diff $(NAME).slice.loc $(TEST_ANS)`): empty diff — files identical.

## Diff against golden
Empty — no differences. The golden file is byte-identical to the 3.4 master
base (`86f3b8a`); no regeneration was needed or performed.

## Root causes
None. The test passes cleanly on the LLVM 8.0.0 port.

## Proposed fix
None required.
