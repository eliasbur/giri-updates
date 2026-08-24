# pca-pthread

- **Verdict:** CLEAN (manual run; the automated suite runs `pca-seq`)
- **Variant:** pthread (`TEST_PARALLELISM=pthread`, run manually)
- **Golden file:** ans-inst-pthread.txt (34 lines)   **Criterion:** `calc_mean 58` (criterion-inst-pthread.txt)
- **Diff:** empty — `pca-pthread.slice.loc` identical to the 34-line golden
- **Root cause:** none

## What the test does
`pca-pthread.c` runs principal-component analysis on a 2-D data set with a
map/reduce pthread design: `map` computes the per-thread mean, `reduce_mean`
merges means, `calc_cov`/`reduce_cov` compute and merge the covariance, and
`calc_mean` (the criterion function) computes the final mean vector. The 3.4-era
golden `ans-inst-pthread.txt` is pristine (verified `git diff 86f3b8a..HEAD` shows
no `ans-*.txt` changes).

## Run
Stages 1–6 (build, instrument, trace, assemble, link): silent. Stage 7 (run traced
binary): completes normally, no `[GIRI] Abnormal termination` marker. Stage 8
(`opt -dgiri -criterion-inst=criterion-inst-pthread.txt`): routine `Start slicing
Function:Instruction is defined as calc_mean:58`. Stage 10
(`diff pca-pthread.slice.loc ans-inst-pthread.txt`): **empty diff** — 34 lines
identical.

## Note
On the 5.0.2 port the same pthread variant was FAIL-BUG (8 lines missing) before
commit `3b26ea6` fixed the `PostDominatorFrontier.cpp:37` early-return bug. That fix
is carried on this branch (the branch was cut from the completed 5.0.2 port), so the
pthread variant passes cleanly here too — re-verified on 8.0.0.

## Root causes
None. The test passes cleanly on the LLVM 8.0.0 port.

## Proposed fix
None required.
