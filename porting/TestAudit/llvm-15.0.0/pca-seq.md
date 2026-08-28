# pca-seq (test/pca)

- **Verdict:** CLEAN
- **Golden file:** ans-inst-seq.txt (17 lines)   **Input:** 16
- **Criterion:** -criterion-inst=criterion-inst-seq.txt (CRITERION_TYPE=inst, TEST_PARALLELISM=seq; `calc_mean 51`)
- **EXPECTED_EXIT:** -1   **EXIT_UNCHECKED:** 0
- **Diff:** empty — `$(NAME).slice.loc` identical to golden (the harness `diff` exits 0; independently re-verified by the standalone-`tracer` validation: 17 slice locs == golden, prtrace OK)
- **Root cause:** none — test passes cleanly on LLVM 15.0.0 (new pass manager)

## What the test does
Program (`pca-seq`) compiled with `clang -g -O0 -c -emit-llvm` (clang/LLVM 15.0.0),
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
  …
```

Stages 7-10 (run traced binary, `opt -passes=... dgiri` slice, exit-status +
`[GIRI] Abnormal termination` check, `diff $(NAME).slice.loc $(TEST_ANS)`): the diff
exits 0 (PASS); the slicing stage prints `Start slicing Function:Instruction is defined as calc_mean:51`; and no `[GIRI] Abnormal termination` line appears in the captured log.
