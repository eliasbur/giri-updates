# pca-seq (pca)

- **Verdict:** CLEAN
- **Variant:** seq (Makefile yields to `TEST_PARALLELISM=seq` from the Dockerfile env)
- **Golden file:** ans-inst-seq.txt (17 lines)   **Input:** (see per-test `INPUT` in `test/pca/Makefile`)
- **Criterion:** criterion-inst-seq.txt (`calc_mean 51`)
- **EXPECTED_EXIT:** -1 (default)   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (legacy PM)

## What the test does
See `test/pca/README.md` (3.4-era, unchanged). Program (`pca-seq.c`) compiled with
`clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0), linked (`llvm-link`), instrumented with the
`-trace-giri` pass under the legacy pass manager (`opt -enable-new-pm=0`), traced, then sliced with
`-dgiri` against the criterion; the extracted `file:line` list is diffed against the 3.4-era golden,
which is pristine (verified: `git diff 224bdfb..HEAD -- test/` shows no `ans-*.txt` / criterion changes).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -enable-new-pm=0 -trace-giri`, llc, clang++ link
against `librtgiri.a`): silent, no warnings (BUILD stage in the per-test log).

Stage 7-10 (run traced binary, `opt -enable-new-pm=0 -dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`), as captured in the
evidence run log (`test/_test_logs/pca.log`, commit 74b870f):

```
Program stdout (traced run):
  Number of rows = 10
  Number of cols = 10
  Max value for each element = 100
     83    86    77    15    93    35    86    92    49    21 
     62    27    90    59    63    26    40    26    72    36 
     11    68    67    29    82    30    62    23    67    35 
     29     2    22    58    69    67    93    56    11    42 
     29    73    21    19    84    37    98    24    15    70 
     13    26    91    80    56    73    62    70    96    81 
      5    25    84    27    36     5    46    29    13    57 
     24    95    82    45    14    67    34    64    43    50 
     87     8    76    78    88    84     3    51    54    99 
     32    60    76    68    39    12    26    86    94    39 
    938    28   256   -13   288  -423    66   -24  -474    46 
     28   495   180  -185  -263   167   192  -146   246   224 
    256   180   591   -82   338   124   239    38  -303   121 
    -13  -185   -82   829   365    98    21  -353    14  -424 
    288  -263   338   365   956  -275   164  -163  -397  -481 
   -423   167   124    98  -275   725   273    80   202   303 
     66   192   239    21   164   273   604   148    16   120 
    -24  -146    38  -353  -163    80   148   645  -270   214 
   -474   246  -303    14  -397   202    16  -270  1132  -135 
     46   224   121  -424  -481   303   120   214  -135   757 
Slice-stage progress:
  Start slicing Function:Instruction is defined as calc_mean:51
```