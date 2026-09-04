# kmeans-seq (kmeans)

- **Verdict:** CLEAN
- **Variant:** seq (`TEST_PARALLELISM=seq` via `Dockerfile` env)
- **Golden file:** ans-inst-seq.txt (**2** source-line entries)   **Pristine:** yes (no `ans-*.txt` change; `git diff 224bdfb..HEAD -- test/` is empty)
- **Criterion:** `criterion-inst=criterion-inst-seq.txt` (`main 120`)
- **Diff:** empty — `$(NAME).slice.loc` identical to the 3.4-era golden

## What the test does
Program compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 12.0.0), instrumented
with the `-trace-giri` pass (both pipeline stages apply the identical
`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum` sequence,
`test/Makefile.common`), traced by the real program, then sliced with `-dgiri`
against the instruction-number criterion. The extracted `file:line` list
(`sed -n '/^Source.*[0-9]+$/p' | awk -F: '{print $3}' | sort -g | uniq`) is
diffed against the 3.4-era golden, which is pristine.

## Stage-by-stage output (honest harness, per-test `_test_logs/kmeans.log`)
Stages 1–6 (clean, `clang`, `llvm-link`, `opt -trace-giri`, `llc`, `clang++` link
against `librtgiri.a`): silent, no warnings (test20 carries a pre-existing,
unchanged `implicit declaration` note; no `-Wno-error` is applied on 12.0.0 and
it compiles cleanly).

Stage 7 (run traced binary):
```
 ===
Dimension = 3
Number of clusters = 10
Number of points = 100
Size of each dimension = 10
Generating points
Generating means
Starting iterative algorithm
..........
Final Means:
    3     1     3 
    1     5     8 
    6     7     6 
    5     7     1 
    6     2     6 
    7     3     1 
    
```

No `[GIRI] Abnormal termination` marker. seq variant; `Dimension = 3 Number of clusters = 10`; 2-line slice matches the 3.4 golden exactly.

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
