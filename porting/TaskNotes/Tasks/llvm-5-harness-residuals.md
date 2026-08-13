---
title: Close the three honesty holes the EXPECTED_EXIT mechanism left behind.
status: open
priority: medium
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-13
---

## Goal

Make `EXIT_UNCHECKED` mean "the exit *status* is not checked" instead of "nothing about this test
is observed", and make the two places that describe the exit-status mechanism
(`test/Makefile.common`'s own comment and `porting/AgentGuide.md`) say what the code actually does.

## Why this task exists

`llvm-5-harness-fallout` (merged as `5fbca9d`) built the per-test opt-in that
`llvm-5-harness-honesty` specified, and it works: 21 of 22 tests are scored honestly against their
goldens. Three residuals in that landing are wrong in the same direction the honesty task existed to
fix — they hide output or misdescribe the harness — and no open task is allowed to touch
`test/Makefile.common`, so they need an owner.

### Residual 1 — `EXIT_UNCHECKED` discards the program's stdout and stderr

`test/Makefile.common:51-52`:

```make
ifeq (1,$(EXIT_UNCHECKED))
	@ ./$< ${INPUT} > /dev/null 2>&1 || true
```

Both redirections and the `|| true` are wider than the task asked for. The honesty task removed a
global `-` prefix precisely so that a traced binary's output would reach the per-test log; this
branch sends it to `/dev/null` instead. It is verifiable in the retained logs: `test3`'s log
contains `fibonacci(15) is 610`, `test9`'s log contains nothing between its two stage markers.

The consequence is not cosmetic. `[GIRI] Abnormal termination, signal number 6` — the runtime
message that revealed the kmeans crash in the first place — goes to stderr. Under this branch, a
signal death of the traced binary produces an empty log and a silent continue. That is the exact
failure mode `llvm-5-harness-honesty` was written to eliminate, re-introduced for one test.

Only `test/UnitTests/test9/Makefile` sets `EXIT_UNCHECKED = 1` today, so the blast radius is one
test — but the mechanism is what the next agent will copy.

### Residual 2 — `AgentGuide.md` states a guarantee the harness does not give

`porting/AgentGuide.md`, "Declaring an acceptable exit status":

> A crash (signal death, `opt` segfault) always produces `[FAIL]` regardless of these settings.

False for the traced binary under `EXIT_UNCHECKED=1`, per Residual 1. It is true for `opt`, and
true for every test using `EXPECTED_EXIT`. Either fix Residual 1 so the sentence becomes true (the
preferred order), or narrow the sentence. Do not leave both as they are.

### Residual 3 — the `EXPECTED_EXIT` comment contradicts the code

`test/Makefile.common:15-16`:

```make
# EXPECTED_EXIT: expected exit code of traced binary (-1 = don't check, any exit allowed)
EXPECTED_EXIT ?= -1
```

The recipe does the opposite: with `EXPECTED_EXIT` at its `-1` default, a non-zero exit fails the
stage (`test/Makefile.common:60-61`). `AgentGuide.md` describes the real behaviour correctly ("With
no declaration (default), a non-zero exit from the traced binary causes `[FAIL build]`"), so the
comment is the wrong one of the two. An agent that trusts it will conclude the default is
permissive and mis-diagnose the next exit-status failure.

## What to do

Keep this small. It is a three-line change plus documentation; it is not an invitation to redesign
the harness.

1. **Residual 1.** Run the binary the same way the checked branch does — output to the log,
   `$(INPUT)` unquoted as elsewhere — and suppress only the *status*, and only for a status that
   came from a normal exit. A `_rc` above 128 means a signal death and must still fail the stage;
   test9's undefined `sum` produces a value in `0..255` from a normal `return`, not a signal, so
   nothing legitimate is lost. Write the reasoning next to the code in one line.
2. **Residual 3.** Correct the comment to match the recipe, and name the two values a test can
   declare.
3. **Residual 2.** Reconcile the AgentGuide sentence with whatever step 1 leaves true, and add the
   fact that `EXIT_UNCHECKED` keeps program output in the log.
4. Re-run the full suite. The expected result is unchanged: **21 PASS / 1 FAIL**
   (`matrix_multiply-seq`). Any other change is a finding and goes in the progress log.

## Definition of done

- [ ] `EXIT_UNCHECKED=1` keeps the traced binary's stdout and stderr in the per-test log —
      demonstrated by `test9`'s log containing its program output after the change
- [ ] A signal death of the traced binary still produces `[FAIL]` under `EXIT_UNCHECKED=1` —
      demonstrated by temporarily making test9 abort (e.g. a `kill -SEGV`-equivalent edit to a
      *copy*, or an injected `EXIT_UNCHECKED=1` on a test made to segfault), then reverting
- [ ] `test/Makefile.common`'s `EXPECTED_EXIT` comment matches the recipe's behaviour
- [ ] `porting/AgentGuide.md`'s crash-guarantee sentence is true as written
- [ ] Full suite re-run: 21 PASS / 1 FAIL, same failure as before, and no test's verdict changed
- [ ] No `ans-*.txt`, no criterion file, no `test/auto-tests.txt`, and no source under `lib/`,
      `include/`, `runtime/` or `tools/` modified
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"):

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-residuals --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-residuals bash -lc 'source /giri/utils/build.sh'
```

`--cpuset-cpus=0-3` is not cosmetic: on the 256-CPU host a `kmeans-pthread` run aborts and writes a
108 GB trace. The suite runs the seq variants, but keep the restriction anyway.

Single test, without the suite wrapper cleaning up underneath you:

```bash
docker exec giri-residuals make -s -C /giri/test/UnitTests/test9 DEBUGFLAGS=
docker exec giri-residuals make test -s -C /giri/test/UnitTests/test9 DEBUGFLAGS=
```

## Traps

- `test/Makefile.common` affects all 22 tests. Re-run the whole suite after every edit to it, not
  just test9.
- Do not widen this into "improve the harness". `matrix_multiply-seq` is not yours — it belongs to
  `llvm-5-seq-variant-failures`, and it must still be the one failing test when you are done.
- Exit statuses are truncated to 8 bits, and `$?` reports `128 + signal` for a signal death. Those
  two facts are what makes step 1's distinction possible; do not conflate them.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain, as are Giri's own
  `assert()`s (`Release` CMake build). Test programs' `assert()`s do fire.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `test/Makefile.common` — the `EXIT_UNCHECKED` branch and the `EXPECTED_EXIT` comment
- `porting/AgentGuide.md` — the exit-status section
- `porting/TaskNotes/Tasks/llvm-5-harness-residuals.md` (this note — progress log)
- Read-only: `test/**` apart from `Makefile.common`, `lib/**`, `include/**`, `runtime/**`,
  `tools/**`, `Dockerfile`, `CMakeLists.txt`, `utils/**`

## Blocked by

- ~~llvm-5-harness-honesty~~
- ~~llvm-5-harness-fallout~~

Run this **before** `llvm-5-seq-variant-failures` if both are queued: it edits a file that scores
every test, and it is short. It must not run concurrently with any task that runs the suite.

## Progress log

## Handoff
- branch `agent/llvm-5-harness-residuals`
Refs: `porting/TaskNotes/Tasks/llvm-5-harness-fallout.md`,
`porting/TaskNotes/Tasks/llvm-5-harness-honesty.md`, `porting/AgentGuide.md`,
`test/Makefile.common`, `AGENTS.md`
