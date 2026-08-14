# kmeans

- **Verdict:** FAIL-HARNESS
- **Golden file:** ans-inst-pthread.txt   **Input:** 16   **Criterion:** -criterion-inst=criterion-inst-pthread.txt (instruction 402, an LLVM instruction index in `main`, not a source line)
- **Diff:** cannot compute (stage 8 timed out)
- **Root cause:** 256-CPU container causes assertion `num_threads == num_procs` to fail at kmeans-pthread.c:316, producing 108 GB trace file that makes stage 8 hang; additionally criterion line 402 exceeds source file length (362 lines)

## What the test does
**Source:** kmeans-pthread.c (362 lines) — parallel k-means clustering using pthreads. Generates random points in a d-dimensional grid, then iteratively assigns points to nearest cluster centers and recomputes means using `num_procs = sysconf(_SC_NPROCESSORS_ONLN)` threads. **Criterion:** instruction 402 in `main` — this is an LLVM instruction index (not a source line number). **Golden:** 78 annotated source lines representing the expected backward slice.

## Stage-by-stage output
1. **make clean** — exit 0, no output.
2. **clang compile** — exit 0, no output.
3. **llvm-link** — exit 0, no output.
4. **opt -trace-giri** — exit 0, no output.
5. **llc** — exit 0, no output.
6. **clang++ link** — exit 0, no output.
7. **./kmeans-pthread.trace.exe 16** — **Bus error (exit 135).** Stderr:
   ```
   kmeans-pthread.trace.exe: kmeans-pthread.c:316: int main(int, char **): Assertion `num_threads == num_procs' failed.
   [GIRI] Abnormal termination, signal number 6
   [GIRI] Abnormal termination, signal number 11
   ```
   Trace file grew to 108 GB (108191924226 bytes). Stdout printed setup info then ".".
8. **opt -dgiri (slicing)** — **timed out** after 120 s. Trace file was 108 GB; opt could not finish reading it. stderr contained no "Could not find Control-dep" messages.
9. **sed/awk on slice** — not reached (no slice file produced).
10. **diff** — not reached.

## Diff against golden
Not computable — stages 8-10 failed. Golden file contains 78 line numbers (81–348). Criterion file `criterion-inst-pthread.txt` contains `main 402` but `kmeans-pthread.c` has only 362 lines.

## Root causes
1. **CPU count mismatch (primary):** The container has 256 CPUs (`nproc` = 256). With the default 100 points, `num_per_thread = 100/256 = 0` and `excess = 100 % 256 = 100`, so only 100 threads are created instead of the expected 256. The assertion `num_threads == num_procs` at kmeans-pthread.c:316 fires.
2. **Unbounded trace growth:** Giri's runtime continued writing trace data after the assertion failure, producing a 108 GB trace file that prevents the slicing stage from completing.
3. **Criterion is an instruction index, not a source line:** `criterion-inst-pthread.txt` specifies `main 402`. The `-criterion-inst` flag takes an LLVM instruction index within a function, not a source line number. The `402` exceeds the source file length (362 lines) because it's the 402nd instruction in `main`, not line 402 of the source.

## Proposed fix
None. This test requires either (a) limiting `num_procs` to a value compatible with 100 points, or (b) switching to the sequential variant (`kmeans-seq`) which does not use the `num_threads == num_procs` assertion. The criterion line 402 also needs to be corrected to a valid location in the current source (362 lines). This is a harness/environment incompatibility, not a bug in Giri's port.