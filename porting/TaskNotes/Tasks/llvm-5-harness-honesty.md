---
title: Make the test harness report real failures instead of hiding them.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-12
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
- `test/Makefile.common:45` — `- ./$< $(INPUT)` runs the instrumented binary with a leading `-`, so
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

- [ ] A stage that fails, crashes, or exits non-zero causes `[FAIL]`; demonstrated by deliberately
      breaking one test (e.g. a bad criterion) and watching the suite report it, with the broken
      state reverted afterwards
- [ ] Per-test stage logs (stdout **and** stderr) are retained for a suite run, via the existing
      `TEST_LOG` hook
- [ ] Pipeline rules, `opt` flags, criteria and the `diff … $(TEST_ANS)` comparison are unchanged in
      intent — the diff of `test/Makefile*` touches only failure visibility
- [ ] `make -C test` with no arguments still runs the suite from `auto-tests.txt` and still prints
      the one-line-per-test table
- [ ] Full suite re-run under the honest harness; the per-test table recorded in the progress log
      and compared against the current 21 PASS / 1 FAIL
- [ ] Every verdict that changed is explained and routed to a task; none is silently absorbed
- [ ] The harness behaviour (what is logged, where, how to get verbose output) documented in
      `porting/AgentGuide.md`
- [ ] No `ans-*.txt`, no criterion file and no source file under `lib/`, `include/`, `runtime/` or
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

## Handoff
- branch `agent/llvm-5-harness-honesty`
Refs: `test/Makefile`, `test/Makefile.common`, `porting/TestAudit/llvm-5.0.2/SUMMARY.md`,
`porting/TaskNotes/Tasks/llvm-5-test-audit.md`, `AGENTS.md`
