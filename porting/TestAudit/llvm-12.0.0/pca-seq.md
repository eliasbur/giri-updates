# pca-seq (pca)

- **Verdict:** CLEAN
- **Variant:** seq (`TEST_PARALLELISM=seq` via `Dockerfile` env)
- **Golden file:** ans-inst-seq.txt (**17** source-line entries)   **Pristine:** yes (no `ans-*.txt` change; `git diff 224bdfb..HEAD -- test/` is empty)
- **Criterion:** `criterion-inst=criterion-inst-seq.txt` (`calc_mean 51`)
- **Diff:** empty — `$(NAME).slice.loc` identical to the 3.4-era golden

## What the test does
Program compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 12.0.0), instrumented
with the `-trace-giri` pass (both pipeline stages apply the identical
`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum` sequence,
`test/Makefile.common`), traced by the real program, then sliced with `-dgiri`
against the instruction-number criterion. The extracted `file:line` list
(`sed -n '/^Source.*[0-9]+$/p' | awk -F: '{print $3}' | sort -g | uniq`) is
diffed against the 3.4-era golden, which is pristine.

## Stage-by-stage output (honest harness, per-test `_test_logs/pca.log`)
Stages 1–6 (clean, `clang`, `llvm-link`, `opt -trace-giri`, `llc`, `clang++` link
against `librtgiri.a`): silent, no warnings (test20 carries a pre-existing,
unchanged `implicit declaration` note; no `-Wno-error` is applied on 12.0.0 and
it compiles cleanly).

Stage 7 (run traced binary):
```
 ===
Number of rows = 10
Number of cols = 10
Max value for each element = 100
   83    86    77    15    93    35    86    92    49    21 
   62    27    90    59    63    26    40    26    72    36 
   11    68    67    29    82    30    62    23    67    35 
   29     2    22    58    69    67    
```

No `[GIRI] Abnormal termination` marker. seq variant; `Number of rows = 10 Number of cols = 10`; 17-line slice matches the 3.4 golden exactly.

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
