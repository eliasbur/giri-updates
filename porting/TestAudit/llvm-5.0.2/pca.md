# pca

- **Verdict:** FAIL-EXPECTED
- **Golden file:** ans-inst-pthread.txt   **Input:** 16   **Criterion:** -criterion-inst=criterion-inst-pthread.txt
- **Diff:** 8 lines missing / 0 lines extra
- **Root cause:** Known bug `PostDominatorFrontier.cpp:37` — 40 "Could not find Control-dep" failures prevent complete backward slice through `pthread_create` call site.

## What the test does

**Source:** `pca-pthread.c` — computes principal-component-analysis-style statistics (mean + covariance matrix) on a random integer matrix (10×10 with values 0–100) using pthreads.

**Computation:** Two phases: (1) `pthread_mean()` spawns N threads, each calling `calc_mean()` to compute column-wise row means; (2) `pthread_cov()` spawns N threads calling `calc_cov()` to compute pairwise covariances. The program prints the generated matrix, computed means, and covariance matrix to stdout.

**Criterion:** `calc_mean:58` — the instruction at line 58 of `calc_mean`, specifically `mean_arg_t *mean_arg = (mean_arg_t *)arg;` (pointer cast of the thread argument). The correct slice should include all statements that data-flow or control-flow depend on this point, spanning: initializations (lines 69–71), `sysconf` call (193), the thread-setup loop in `pthread_mean` (196–216), the `calc_mean` body (140–147), and the `generate_points` inner loop (125–129).

## Stage-by-stage output

| Stage | Result | Notes |
|-------|--------|-------|
| 1. `make clean` | OK | No output |
| 2. `clang -c -emit-llvm` | OK | No output |
| 3. `llvm-link` | OK | No output |
| 4. `opt -trace-giri` | OK | No stderr |
| 5. `llc -O0` | OK | No output |
| 6. `clang++ link` | Needed `-lpthread` | Makefile provides `LDFLAGS=-lpthread`; manual invocation required the flag |
| 7. `./pca-pthread.trace.exe 16` | Exit 0 | Produced matrix + covariance output to stdout |
| 8. `opt -dgiri` | Exit 0, **39× then 1× "Could not find Control-dep"** | stderr: `Start slicing Function:Instruction is defined as calc_mean:58` followed by 40 occurrences of `Could not find Control-dep of this Basic Block`, then `For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later` / `i8* %0` |
| 9. `sed \| awk \| sort \| uniq` | OK | Produced 26-line `pca-pthread.slice.loc` |
| 10. `diff` vs golden | **FAIL (exit 1)** | 8 lines in golden absent from computed slice, 0 extra |

## Diff against golden

```
0a1,3
> 51
> 52
> 53
13a17
> 196
15a20
> 204
19a25
> 211
20a27
> 213
21a29
> 216
```

Missing lines in source context:
- 51: `pthread_mutex_t row_lock;`
- 52: (blank)
- 53: `/* Structure that stores the rows`
- 196: `tid = (pthread_t *)MALLOC(num_procs * sizeof(pthread_t));`
- 204: `int excess = num_rows - (rows_per_thread * num_procs);`
- 211: `if (excess > 0) {`
- 213: `excess--;`
- 216: `CHECK_ERROR(pthread_create(&tid[i], &attr, calc_mean,`

## Root causes

1. **Known bug — `PostDominatorFrontier.cpp:37`** (early return at virtual root): Causes 40 "Could not find Control-dep of this Basic Block" errors during slicing. This is the documented bug. Lines 51-53 are global declarations/comments that are arguably debatable in the slice, but the 5 lines in the `pthread_mean` function (196, 204, 211, 213, 216) are clearly on the control-dep path from `calc_mean` back through the `pthread_create` call site. The missing control dependencies cause the slice algorithm to drop the thread-creation loop's setup logic from `pthread_mean`.

2. **"Variable length functions" warning**: The `pthread_create` call passes `calc_mean` as a function pointer — a variable-length / indirect call pattern the slicer explicitly does not handle: `For some variable length functions like ap_rprintf in apache, call records missing. Stop here for now. Fix it later`. This further contributes to the incomplete backward slice.

## Proposed fix

No fix proposed. This is the known `PostDominatorFrontier.cpp:37` bug with a documented limitation around variable-length function calls. The 8 missing lines (especially the 5 in `pthread_mean`) are a direct consequence of the control-dependency computation failing for basic blocks dominated by or reachable through function-pointer call sites.