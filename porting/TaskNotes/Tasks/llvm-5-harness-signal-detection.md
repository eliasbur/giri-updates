---
title: Make the harness detect a traced binary's abnormal termination, which its exit status cannot express.
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

Give the harness a way to tell "the traced program crashed" apart from "the traced program returned
a small number", so that no test — declared, defaulted or unchecked — can score PASS on a crash.

## Why this task exists

`llvm-5-harness-residuals` (merged as `f8b7d33`) fixed two of its three residuals cleanly: program
output is preserved under `EXIT_UNCHECKED`, and the `EXPECTED_EXIT` comment now matches the recipe.
Its third claim does not hold.

`test/Makefile.common:52-54` now reads:

```make
ifeq (1,$(EXIT_UNCHECKED))
	@ ./$< $(INPUT); \
		_rc=$$?; \
		if [ "$$_rc" -ge 128 ]; then exit $$_rc; fi # signal death (>=128) fails
```

**`_rc` is never ≥ 128 for a traced binary.** Every instrumented program calls `recordInit`, which
installs Giri's own handler for the fatal signals (`runtime/Giri/Tracing.cpp:272-280`):

```c
static void cleanup_only_tracing(int signum) {
  ERROR("[GIRI] Abnormal termination, signal number %d\n", signum);
  exit(signum);                       // Tracing.cpp:253-256
}
```

`exit(signum)` — so a segfault becomes a **normal** exit with status 11, an abort becomes 6. The
shell's `128 + n` convention only applies to a process that actually dies by a signal, and none of
these do. The guard is unreachable.

The task's own progress log records the demonstration that should have caught this:

> Signal death test: modified forloop.c to raise(SIGSEGV), trace generated then opt crashed during
> slicing → [FAIL] as expected.

If the guard had fired, `make` would have stopped at the `.trace` stage and `opt` would never have
run. That the run reached slicing is the evidence that `_rc` was 11 and the check passed the stage;
the `[FAIL]` came from `opt` choking on the truncated trace, which is luck, not detection. A program
that crashes after writing a coherent trace prefix scores PASS.

### The wider ambiguity

This is not only about `EXIT_UNCHECKED`. A traced binary's exit status in `1..31` is ambiguous: it
may be `main`'s return value or a signal number reported by Giri's handler. Today's declarations
are mostly clear of the handled set (2, 3, 4, 6, 8, 11, 15), but **`EXPECTED_EXIT = 2` is declared
by test2, test13, test14 and test16**, and 2 is SIGINT. No current run sends SIGINT, so there is no
live false PASS — but nothing prevents the next one, and nothing tells an agent to check.

The reliable signal is on stderr and is now (since `f8b7d33`) captured in every per-test log:
`[GIRI] Abnormal termination, signal number <n>`, printed by `ERROR`, which is
`fprintf(stderr, …)` unconditionally (`Tracing.cpp:41`) — not compiled out by the `Release` build.

## What to do

### 1. Pick the mechanism, and write down why

- **A — detect the runtime's message in the harness (recommended).** The trace recipe captures the
  binary's output, fails the stage if `[GIRI] Abnormal termination` appears in it, and passes the
  output through to the log either way. Works for every test regardless of `EXPECTED_EXIT` or
  `EXIT_UNCHECKED`, changes no traced-program behaviour, and stays inside `test/`. The one thing to
  get right is that the output must still reach the log unchanged — a `tee` or a temp file, not a
  swallow. Verify with `test3` that normal program output is untouched.
- **B — make the runtime re-raise instead of `exit(signum)`.** `signal(signum, SIG_DFL); raise(signum);`
  after the flush is the correct Unix behaviour and would make `128 + n` real, which is what the
  current guard already assumes. But it changes the observable behaviour of every traced program on
  the port — `kmeans-pthread`'s abort would go from exit 6 to 134 — and `runtime/` is a ported
  component whose behaviour the port is supposed to preserve. If you choose it, say so explicitly,
  and record it as a deliberate behavioural change rather than a bug fix.
- **C — both**, with A as the harness-side guarantee and B raised as a recommendation to the
  project owner rather than applied.

A is the default. Choosing B or C requires an argument in this note.

### 2. Guard the ambiguous declarations

`EXPECTED_EXIT` values that collide with a handled signal number (2, 3, 4, 6, 8, 11, 15) must not
pass silently. At minimum, document the collision next to each such declaration; better, have
`Makefile.common` note the ambiguity where the value is compared. The four `EXPECTED_EXIT = 2`
tests are the concrete cases — do not change their values, they are correct, just make the
collision visible.

### 3. Fix the documentation to match

`porting/AgentGuide.md` currently carries a block describing this defect and pointing at this task
(added by the head agent when it was found). Replace it with an accurate description of whatever
step 1 leaves true.

## Definition of done

- [ ] A traced binary that dies on SIGSEGV produces `[FAIL]` at the **trace stage**, under
      `EXIT_UNCHECKED=1` and under `EXPECTED_EXIT`, demonstrated by a temporary edit to a test
      program that is reverted afterwards. "The next stage failed too" is not the demonstration —
      show the trace stage failing, with the message the harness printed
- [ ] The same demonstration repeated for a crash that leaves a *usable* trace, so the pass is not
      credited to a downstream stage failing by luck. `raise(SIGSEGV)` at the very end of `main`,
      after the traced work is done, is the case to construct
- [ ] Normal program output still reaches the per-test log unchanged — `test3`'s log still contains
      `fibonacci(15) is 610`
- [ ] The chosen mechanism recorded here with its reasoning; if the runtime was changed, the
      behavioural consequence for every traced program is written down and `AGENTS.md` says so
- [ ] The `EXPECTED_EXIT` / signal-number collision made visible for the four tests declaring `2`
- [ ] `porting/AgentGuide.md`'s exit-status section true as written, and its pointer to this task
      removed
- [ ] Full suite re-run: **21 PASS / 1 FAIL**. The one failure is `matrix_multiply-seq`, whose
      segfault was fixed in `3945134` but whose diff against the golden remains — see
      `llvm-5-matrix-multiply-verdict`. Any other change is a finding
- [ ] No `ans-*.txt`, no criterion file, no `test/auto-tests.txt` modified, and no permanent change
      to any test's `.c` source
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"):

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-signals --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-signals bash -lc 'source /giri/utils/build.sh'
```

`--cpuset-cpus=0-3` is not cosmetic: on the 256-CPU host a `kmeans-pthread` run aborts and writes a
108 GB trace. Rebuild with `make -j$(nproc) -C /giri/build`, not `docker build`.

Single test, without the suite wrapper cleaning up underneath you:

```bash
docker exec giri-signals make -s -C /giri/test/UnitTests/test9 DEBUGFLAGS=
docker exec giri-signals make test -s -C /giri/test/UnitTests/test9 DEBUGFLAGS=
```

If you choose mechanism B, `runtime/` is rebuilt by the same `make -C /giri/build`; the traced
binaries link `librtgiri.a` statically, so every test must be relinked — `make clean -C /giri/test`
before the suite run, or you will measure stale executables.

## Traps

- **The `128 + n` convention does not apply here.** That assumption is what produced this task; do
  not reintroduce it in a different spot.
- `signal(SIGKILL, …)` at `Tracing.cpp:278` is a no-op — SIGKILL cannot be caught. Pre-existing and
  harmless; do not "fix" it as part of this task, and do not build a test around SIGKILL.
- `test/Makefile.common` affects all 22 tests. Re-run the whole suite after every edit to it.
- Anything you edit in a test's `.c` file to force a crash must be reverted, and the revert shown in
  the diff. `llvm-5-harness-residuals` did this correctly — follow it.
- `matrix_multiply-seq` is not yours; it belongs to `llvm-5-matrix-multiply-verdict`. Its failure is
  now a non-empty diff at the `test` stage, which the harness already reports correctly.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain, as are Giri's own
  `assert()`s (`Release` CMake build). Test programs' `assert()`s do fire — that is how kmeans
  aborts.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `test/Makefile.common` — the trace recipe
- `test/UnitTests/test{2,13,14,16}/Makefile` — the `EXPECTED_EXIT = 2` collision comments
- `runtime/Giri/Tracing.cpp` — **only** if mechanism B or C is chosen and argued for
- `porting/AgentGuide.md` — the exit-status section
- `AGENTS.md` — only if the runtime behaviour changes
- `porting/TaskNotes/Tasks/llvm-5-harness-signal-detection.md` (this note — progress log)
- Read-only: `test/**` apart from the Makefiles above, `lib/**`, `include/**`, `tools/**`,
  `Dockerfile`, `CMakeLists.txt`, `utils/**`

## Blocked by

- ~~llvm-5-harness-honesty~~
- ~~llvm-5-harness-fallout~~
- ~~llvm-5-harness-residuals~~

Nothing gates this and it gates nothing: no current test's verdict is wrong because of it. Run it
after `llvm-5-matrix-multiply-verdict` and `llvm-5-kmeans`, before `llvm-5-port-closeout`. It must not
run concurrently with any task that runs the suite.

## Progress log

## Handoff
- branch `agent/llvm-5-harness-signal-detection`
Refs: `porting/TaskNotes/Tasks/llvm-5-harness-residuals.md`,
`porting/TaskNotes/Tasks/llvm-5-harness-fallout.md`, `porting/AgentGuide.md`,
`test/Makefile.common`, `runtime/Giri/Tracing.cpp`, `AGENTS.md`
