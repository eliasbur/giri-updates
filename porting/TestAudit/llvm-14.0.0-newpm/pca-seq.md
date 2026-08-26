# pca (test/pca)

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt (17 lines)   **Input:** 16
- **Criterion:** -criterion-inst=criterion-inst-seq.txt (CRITERION_TYPE=inst, TEST_PARALLELISM=seq)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0)
- **Root cause:** none — test passes cleanly on LLVM 14.0.0 (new pass manager)

## What the test does
Program (`pca-seq`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 14.0.0),
linked (`llvm-link`), instrumented with the `trace-giri` pass under the **new pass
manager** (`opt -passes="function(mergereturn),bbnum,lsnum,trace-giri,remove-bbnum,remove-lsnum"`,
each Giri library loaded via `-load` + `-load-pass-plugin`), traced, then sliced with
`dgiri` in the same pipeline against the criterion; the extracted `file:line` list is
diffed against the pristine 3.4-era golden (verified: `git diff fba2565..HEAD -- test/`
shows no `ans-*.txt` / criterion changes; only the harness files changed).

## Stage-by-stage output (honest harness, captured `_test_logs/*.log`)
Stages 1-6 (clean, clang, llvm-link, `opt -passes=... trace-giri`, llc, clang++ link
against `librtgiri.a`) and the traced-run stdout, as captured in
`_test_logs/pca.log`:

```
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
  Start slicing Function:Instruction is defined as calc_mean:51
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slice criterion is printed by the slicing stage ("Start slicing
…") and no `[GIRI] Abnormal termination` line appears in the captured log.
