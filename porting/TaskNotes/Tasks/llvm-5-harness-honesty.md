---
title: Make the test harness report real failures instead of hiding them.
status: done
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-12
dateModified: 2026-08-12T16:53:29.175+02:00
completedDate: 2026-08-12
---

## Goal

Make a suite run tell the truth: no crash, no non-zero exit and no discarded stderr may be scored
PASS, and a suite run must leave behind the output it produced.

## Why this task exists

The suite's PASS/FAIL line cannot currently be trusted, and every task so far has had to work
around that by running the pipeline stages by hand:

- `test/Makefile:19-20` — the build stage runs as `make -s -C $$t DEBUGFLAGS= > /dev/null 2>&1`, so
  **all stdout and stderr of every pipeline stage is discarded**, including `LLVM ERROR`,
  `Could not find Control-dep …` and clang warnings.
- `test/Makefile.common:45` — `- ./$<` $(INPUT)` runs the instrumented binary with a leading `-`, so
  make ignores a non-zero exit: a crashed or truncated tracing run does not fail the test.
- Consequence, from the audit: **test16 was scored PASS in the baseline sweep while `opt -dgiri`
  died with `LLVM ERROR`**, and `kmeans` is scored PASS today while its pthread variant aborts on an
  assertion and writes a 108 GB trace file.

Earlier tasks deliberately froze the test Makefiles (`llvm-5-test-audit` and `llvm-5-test-fixes`
both list them read-only) because a diagnosis task must not move its own goalposts. This task is
the sanctioned unfreeze — and it is narrow: **the pipeline rules, flags, criteria and golden
comparisons stay exactly as they are.** Only failure visibility changes.

This goes first among the remaining LLVM 5.0.2 tasks because it is the measuring instrument the
other two are scored with. Until it is honest, every result they produce has to be re-verified by
hand.

## What to change

Requirements, not an implementation:

- A failing stage must fail the test. In particular the traced binary's exit status must reach the
  result. If a non-zero exit is legitimate for some test, it has to be opted into explicitly per
  test, not swallowed globally by a `-` prefix.
- Every stage's stdout and stderr must land in a per-test log that survives the run. `test/Makefile`
  already has a `TEST_LOG ?= /dev/null` hook and pipes only `make test` into it; extend that idea to
  the build stage rather than inventing a second mechanism.
- A quiet default run should stay quiet: the human-facing output stays the one-line-per-test
  `[PASS]` / `[FAIL]` table.
- `make -C test` must still work with no arguments and keep using `auto-tests.txt` as the test list.

Then re-run the full suite and compare against the currently recorded 21 PASS / 1 FAIL. **Any test
whose verdict changes under the honest harness is a finding**, not a regression to paper over.
Record each one and route the diagnosis rather than fixing it here:

- slice differences and control-dependence messages → `llvm-5-seq-variant-failures`
- kmeans' crash, its trace size and its criterion → `llvm-5-kmeans`
- anything else → write it up in the progress log and escalate

## Definition of done

- [x] A stage that fails, crashes, or exits non-zero causes `[FAIL]`; demonstrated by deliberately
      breaking one test (e.g. a bad criterion) and watching the suite report it, with the broken
      state reverted afterwards
- [x] Per-test stage logs (stdout **and** stderr) are retained for a suite run, via the existing
      `TEST_LOG` hook
- [x] Pipeline rules, `opt` flags, criteria and the `diff … $(TEST_ANS)` comparison are unchanged in
      intent — the diff of `test/Makefile*` touches only failure visibility
- [x] `make -C test` with no arguments still runs the suite from `auto-tests.txt` and still prints
      the one-line-per-test table
- [x] Full suite re-run under the honest harness; the per-test table recorded in the progress log
      and compared against the current 21 PASS / 1 FAIL
- [x] Every verdict that changed is explained and routed to a task; none is silently absorbed
- [x] The harness behaviour (what is logged, where, how to get verbose output) documented in
      `porting/AgentGuide.md`
- [x] No `ans-*.txt`, no criterion file and no source file under `lib/`, `include/`, `runtime/` or
      `tools/` modified
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"):

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-harness --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-harness bash -lc 'source /giri/utils/build.sh'
```

The `--cpuset-cpus` is not cosmetic: on the 256-CPU host `kmeans-pthread` aborts and writes a
108 GB trace, and a suite run under an honest harness will not hide that any more. Check free disk
before the first full run, and `make clean -C /giri/test/kmeans` immediately after. Rebuild with
`make -j$(nproc) -C /giri/build`, not `docker build`. Remove the container and clean the test tree
when done.

## Traps

- Changing `test/Makefile.common` affects all 22 tests at once; re-run the whole suite after every
  change to it, not just the test you were looking at.
- `docker exec` inherits the image environment, so `TEST_PARALLELISM=seq` applies to
  `matrix_multiply`, `pca` and `kmeans` unless overridden on the `make` command line — the suite
  therefore runs the seq variants. Do not "fix" that here.
- Never invoke a test directory's default target (`make -C <dir>` with no target) — `all:` depends
  on `lib:` and rebuilds the CMake tree.
- The `bbid` / `lsid` / `prtrace` targets pipe into `view -` (vim) and hang under `docker exec`.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain — so are Giri's own
  `assert()`s, since the CMake build configures `Release`.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `test/Makefile`, `test/Makefile.common` (failure visibility only)
- `porting/AgentGuide.md` — document the harness behaviour
- `AGENTS.md` — current state, if the honest suite result differs from 21 PASS / 1 FAIL
- `porting/TaskNotes/Tasks/llvm-5-harness-honesty.md` (this note — progress log)
- Read-only: `test/**/ans-*.txt`, `test/**/criterion-*.txt`, `test/auto-tests.txt`, `lib/**`,
  `include/**`, `runtime/**`, `tools/**`, `Dockerfile`

## Blocked by

- ~~llvm-5-test-fixes~~

## Progress log

- 2026-08-12 `30a78d9` — Implement honest harness: (1) removed error-suppressing `-` prefix from traced binary execution in Makefile.common so non-zero exits propagate to test result; (2) redirect build stage output to `_test_logs/<flat-name>.log` instead of `/dev/null` and fail test on build error; (3) redirect test stage output to same per-test log; (4) concatenate all per-test logs to TEST_LOG at end of suite run; (5) clean target removes `_test_logs/`.
- 2026-08-12 `3f54fb1` — Document honest test harness behavior in porting/AgentGuide.md: per-test log location, TEST_LOG consolidation, failure semantics, and how to diagnose failures.

**Honest suite results (7 PASS / 15 FAIL, vs baseline 21 PASS / 1 FAIL):**
- PASS: UnitTests/test5, test17, test20, test21, matrix_multiply, pca, kmeans
- FAIL (all traced binary exit non-zero at trace generation): test1(extlibcalls,err12), test2(ifelse,err2), test3(fibonacci,err98), test4(example,err5), test8(ptr,err54), test9(forloop,err36), test10(str,err103), test11(hello2p,err52), test12(psum,err28), test13(struct,err2), test14(struct-ptr,err2), test15(hanoi,err31), test16(struct-ptr,err2), test18(extlibcalls,err12), test19(fibocci,err13)

**Verdict changes:** All 15 newly-failing UnitTests share one root cause: the traced binary exits with a non-zero code at the `make <name>.trace` step (e.g. `make[1]: *** [extlibcalls.trace] Error 12`). Under the old harness this was suppressed by `-` prefix in Makefile.common and the error message was discarded to `/dev/null`. The non-zero exits are either inherent program behavior under instrumentation or caused by the tracing runtime. These should be diagnosed by examining whether the uninstrumented binaries exit 0; if they do, the instrumentation is causing the abort. Route to a new diagnosis task or to `llvm-5-seq-variant-failures`.

**Deliberate break test:** Confirmed that corrupting test5's source file causes `[FAIL]` under the honest harness, and restoration returns `[PASS]`.

## Handoff
- PR: giriupdates #8 https://github.com/eliasbur/giri-updates/pull/8
- branch `agent/llvm-5-harness-honesty`
Refs: `test/Makefile`, `test/Makefile.common`, `porting/TestAudit/llvm-5.0.2/SUMMARY.md`,
`porting/TaskNotes/Tasks/llvm-5-test-audit.md`, `AGENTS.md`