---
title: Settle kmeans on the LLVM 5.0.2 port.
status: open
priority: medium
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-12
---

## Goal

Give `kmeans` — the one test whose result the suite has never actually measured — a decided,
written-down outcome, and correct the two audit claims about it that do not hold up.

## Why this task exists

`kmeans` is scored PASS by the suite while its pthread variant aborts on an assertion and writes a
108 GB trace. It got that PASS from the harness holes fixed in `llvm-5-harness-honesty`; once those
are closed, whatever kmeans really does becomes visible and has to be dealt with rather than
tolerated.

`porting/TestAudit/llvm-5.0.2/kmeans.md` (pthread variant) found:

1. **CPU-count assertion.** `kmeans-pthread.c:316` asserts `num_threads == num_procs` with
   `num_procs = sysconf(_SC_NPROCESSORS_ONLN)`. On the 256-CPU build host with 100 points,
   `num_per_thread` is 0, only 100 threads are created and the assertion fires
   (`[GIRI] Abnormal termination, signal number 6`).
2. **108 GB trace.** The aborted run leaves a 108191924226-byte trace; stage 8 (`opt -dgiri`) then
   cannot finish and timed out at 120 s in the audit.
3. **A claimed out-of-range criterion — this one is wrong.** The report says
   `criterion-inst-pthread.txt`'s `main 402` "exceeds source file length (362 lines)". But
   `-criterion-inst` is "criterion by instruction number" (`lib/Giri/Giri.cpp:50`) and is resolved
   by walking `inst_iterator` (`Giri.cpp:296-303`) — 402 is the 402nd **instruction** of `main`, not
   a source line. `test/matrix_multiply/criterion-inst-seq.txt` (`matrix_mult 285` against a
   180-line source) makes the same point independently.

Neither the audit nor the fixes task ever ran the **seq** variant of kmeans, which is what the suite
actually executes (`Dockerfile:5` sets `TEST_PARALLELISM=seq`, and the test Makefiles' `?=` yields
to the environment). `kmeans-seq` is scored PASS against a **two-line** golden —
`test/kmeans/ans-inst-seq.txt` contains `222` and `276`.

## What to do

**Correct the record (point 3).** Verify how many instructions `main` has in the 5.0.2 build and
whether `main 402` resolves at all; if it does, name the instruction it selects. Then fix the
root-cause line in `porting/TestAudit/llvm-5.0.2/kmeans.md` and the corresponding row and narrative
in `SUMMARY.md`. Note the general risk while you are there: every test's criterion is an instruction
index into clang-generated IR, and the goldens were produced against 3.4's IR —
`llvm-5-seq-variant-failures` is testing that same hypothesis for `matrix_multiply`, so read its
findings before writing yours.

**Verify the seq variant.** Run `kmeans-seq` stage by stage and confirm the two-line golden match is
genuine and not degenerate — an empty or near-empty slice matching a near-empty golden is a false
PASS, and the honest harness should be able to tell the difference. If the match is degenerate, say
so and treat it as a finding about the golden, not about the port.

**Decide the pthread variant (points 1-2).** Implement exactly one outcome and write down why:

- run the suite CPU-restricted (`docker run --cpuset-cpus=…`) and document it as a stated
  environment requirement, or
- give kmeans an input large enough that `num_per_thread > 0` on a many-core host — this changes
  what the trace contains and therefore the slice, so the golden-file relationship has to be
  re-checked, not assumed, or
- remove kmeans from `test/auto-tests.txt` with a written reason and a pointer to this note.

Editing `ans-*.txt` is not one of the options. Whichever is chosen, the reasoning goes in this note
and the consequence in `AGENTS.md`'s `## Current state`.

## Definition of done

- [ ] `kmeans-seq` audited stage by stage, with a report at
      `porting/TestAudit/llvm-5.0.2/kmeans-seq.md` following the schema in `llvm-5-test-audit.md`
      (skip if `llvm-5-seq-variant-failures` already wrote it — cross-reference instead)
- [ ] The two-line golden match on `kmeans-seq` shown to be genuine or degenerate, with the slice
      content as evidence
- [ ] Instruction count of `main` in the 5.0.2 build recorded, and whether `main 402` resolves
- [ ] The "criterion line 402 out of range" claim corrected in
      `porting/TestAudit/llvm-5.0.2/kmeans.md` and in `SUMMARY.md`
- [ ] One outcome implemented for the pthread variant, with its reasoning in this note and its
      consequence in `AGENTS.md`
- [ ] Full suite re-run afterwards under the honest harness; no other test's verdict changed
- [ ] No `ans-*.txt` and no criterion file modified
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-kmeans --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-kmeans bash -lc 'source /giri/utils/build.sh'
```

Select the variant explicitly on the `make` command line — a command-line assignment overrides both
the Makefile default and the container environment:

```bash
docker exec giri-kmeans make -s -C /giri/test/kmeans TEST_PARALLELISM=seq DEBUGFLAGS=
docker exec giri-kmeans make test -s -C /giri/test/kmeans TEST_PARALLELISM=seq DEBUGFLAGS=
```

**Check free disk before running the pthread variant at all**, and `make clean -C /giri/test/kmeans`
immediately after every run — an aborted run leaves a trace measured in tens of gigabytes.

## Traps

- Never invoke a test directory's default target (`make -C <dir>` with no target) — `all:` depends
  on `lib:` and rebuilds the CMake tree.
- The `bbid` / `lsid` / `prtrace` targets pipe into `view -` (vim) and hang under `docker exec`;
  `prtrace` on a 108 GB trace is not a plan either.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain — so are Giri's own
  `assert()`s, since the CMake build configures `Release`. The assertion that fires in kmeans is in
  the **test program**, which the test Makefile compiles without `-DNDEBUG`.
- This note edits `SUMMARY.md`, and so do `llvm-5-seq-variant-failures` and `llvm-5-port-closeout`.
  Run them in sequence, not in parallel.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `porting/TestAudit/llvm-5.0.2/kmeans.md`, `porting/TestAudit/llvm-5.0.2/kmeans-seq.md` (new),
  `porting/TestAudit/llvm-5.0.2/SUMMARY.md`
- `test/auto-tests.txt`, `Dockerfile`, `test/kmeans/Makefile` — only if the chosen outcome requires it
- `AGENTS.md`, `porting/AgentGuide.md` — the run requirement, if one is introduced
- `porting/TaskNotes/Tasks/llvm-5-kmeans.md` (this note — progress log)
- Read-only: `test/**/ans-*.txt`, `test/**/criterion-*.txt`, `lib/**`, `include/**`, `runtime/**`,
  `tools/**`

## Blocked by

- ~~llvm-5-test-fixes~~
- llvm-5-harness-honesty

## Progress log

## Handoff
- branch `agent/llvm-5-kmeans`
Refs: `porting/TestAudit/llvm-5.0.2/kmeans.md`, `porting/TestAudit/llvm-5.0.2/SUMMARY.md`,
`porting/TaskNotes/Tasks/llvm-5-harness-honesty.md`,
`porting/TaskNotes/Tasks/llvm-5-seq-variant-failures.md`, `AGENTS.md`
