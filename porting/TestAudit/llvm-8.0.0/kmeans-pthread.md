# kmeans-pthread

- **Verdict:** FAIL-HARNESS (container CPU count; identical to the 5.0.2 finding)
- **Variant:** pthread (`TEST_PARALLELISM=pthread`, run manually; the automated suite runs `kmeans-seq`)
- **Golden file:** ans-inst-pthread.txt (87 lines)   **Criterion:** `main 402` (criterion-inst-pthread.txt)
- **Diff:** not reached — the traced binary aborts before slicing completes
- **Root cause:** `kmeans-pthread.c:316` asserts `num_threads == num_procs`; the
  container has 256 CPUs while `DEF_NUM_POINTS` is 100, so only 100 threads are
  created and the assertion fires. Pre-existing environmental incompatibility, not a
  port regression.

## Run
Stages 1–6 (build, instrument, trace, assemble, link): silent. Stage 7 (run traced
binary): `Starting iterative algorithm` is printed, then:
```
kmeans-pthread.trace.exe: kmeans-pthread.c:316: int main(int, char **):
  Assertion `num_threads == num_procs' failed.
[GIRI] Abnormal termination, signal number 6
```
The harness's crash detection (established on the 5.0.2 port, commit `e194151`)
catches the `[GIRI] Abnormal termination` marker and fails the stage. Stage 8
(slicing) was started but **timed out**: the binary's runaway pre-crash iteration
wrote a **101 GB** trace file (the 5.0.2 audit measured 108 GB on the same host),
and the 256-thread slicing run did not finish within the 200 s bound. The trace
file was removed after the run.

## Why this is FAIL-HARNESS and not a port bug
- The assertion is in the **test program** (`kmeans-pthread.c:316`), written in the
  3.4 era on an assumption that the point count (100) is >= the CPU count. On a
  256-CPU host that assumption is false; the behavior is identical on the 3.4, 5.0.2,
  and 8.0.0 ports (the 5.0.2 audit documented the exact same failure, same line, same
  108 GB trace).
- The slice criterion `main 402` is an instruction index, not a source line, so even a
  clean run would be subject to the same codegen drift as `matrix_multiply-pthread`;
  that question is moot while the binary aborts.
- The criterion is out of reach because the program aborts at stage 7; no port code is
  exercised past the crash.

## Resolution
None applied. This is an environmental limitation of running the pthread variant on a
>100-CPU host; the automated suite avoids it by running `kmeans-seq` (which PASSES on
8.0.0, see `kmeans-seq.md`). Left as-is to match the 5.0.2 port's handling.

## Proposed fix
None required for the port. (A future cleanup could cap the thread count in the test
program with `min(num_procs, num_points)`, but that changes test source and is out of
scope.)
