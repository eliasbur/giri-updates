---
title: Fix the harness's crash blindness and settle kmeans, in one pass.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-14
---

## Goal

Land the two remaining code-level defects on `port/llvm-5.0.2` in a single container session: the
harness cannot detect a traced binary's crash, and `kmeans-pthread` is an unhandled abort sitting in
the suite list. Both are small; neither needs its own image build.

**This note supersedes `llvm-5-harness-signal-detection` and `llvm-5-kmeans`** (both merged into it
2026-08-14, no work had started on either).

## How to report

Every Definition-of-done item below asks for a **pasted artifact** — a command's actual output, a
diff, a table. Do not tick a box by asserting the property; tick it by pasting what you saw into the
progress log or the report. Two earlier tasks ticked boxes for steps they had not run, and both had
to be redone.

---

# Part 1 — the harness cannot see a traced binary crash

## Why

`test/Makefile.common:51-52`:

```make
ifeq (1,$(EXIT_UNCHECKED))
	@ ./$< $(INPUT); \
		_rc=$$?; \
		if [ "$$_rc" -ge 128 ]; then exit $$_rc; fi # signal death (>=128) fails
```

`_rc` is never ≥ 128. Every instrumented program calls `recordInit`, which installs Giri's handler
for the fatal signals (`runtime/Giri/Tracing.cpp:272-280`), and that handler ends in `exit(signum)`
(`Tracing.cpp:253-256`). A segfault becomes a **normal** exit with status 11; an abort, 6. The
`128 + n` convention applies only to a process that really dies by a signal, and none do. The guard
is unreachable.

`llvm-5-harness-residuals` demonstrated this without noticing: its progress log says "trace generated
then opt crashed during slicing → [FAIL] as expected". Had the guard fired, `make` would have stopped
at the `.trace` stage. The `[FAIL]` came from `opt` choking on a truncated trace — luck. A program
that crashes after writing a coherent trace prefix scores PASS.

The wider ambiguity: a traced binary's exit status in `1..31` may be `main`'s return value **or** a
signal number. `EXPECTED_EXIT = 2` is declared by test2, test13, test14 and test16, and 2 is SIGINT.
No live false PASS today, but nothing flags it.

The reliable marker is on stderr and — since `f8b7d33` — reaches every per-test log:
`[GIRI] Abnormal termination, signal number <n>`, printed by `ERROR`, which is
`fprintf(stderr, …)` unconditionally (`Tracing.cpp:41`), not compiled out by the `Release` build.

## What to do

Pick **A** unless you can argue otherwise:

- **A — detect the runtime's message in the harness.** The trace recipe captures the binary's output,
  fails the stage if `[GIRI] Abnormal termination` appears, and passes the output through to the log
  either way (`tee`, not a swallow). Covers every test regardless of `EXPECTED_EXIT` /
  `EXIT_UNCHECKED`, changes no traced-program behaviour, stays inside `test/`.
- **B — make the runtime re-raise** (`signal(signum, SIG_DFL); raise(signum);` after the flush).
  Correct Unix behaviour, and it makes the existing `128 + n` guard real. But it changes the
  observable behaviour of every traced program — `kmeans-pthread`'s abort goes from 6 to 134 — and
  `runtime/` is a ported component whose behaviour the port preserves. Choosing it is a deliberate
  behavioural change, recorded as such, not a bug fix.

Then make the `EXPECTED_EXIT` / signal-number collision visible for the four tests declaring `2` —
a comment at each declaration, or a note in `Makefile.common` where the comparison happens. Do not
change their values; they are correct.

---

# Part 2 — kmeans

## Why

`kmeans-seq` passes the honest harness against a **two-line** golden (`ans-inst-seq.txt` = `222`,
`276`); `porting/TestAudit/llvm-5.0.2/kmeans-seq.md` records it CLEAN with `main 120` resolving and
an empty diff. `kmeans-pthread` has not been run since the pre-`3b26ea6` audit, which found
(`porting/TestAudit/llvm-5.0.2/kmeans.md`):

1. **CPU-count assertion.** `kmeans-pthread.c:316` asserts `num_threads == num_procs` with
   `num_procs = sysconf(_SC_NPROCESSORS_ONLN)`. On the 256-CPU host with 100 points,
   `num_per_thread` is 0, only 100 threads are created, and the assertion fires.
2. **108 GB trace.** The aborted run leaves 108191924226 bytes; `opt -dgiri` then cannot finish.
3. **A wrong claim to correct.** The report says `criterion-inst-pthread.txt`'s `main 402` "exceeds
   source file length (362 lines)". `-criterion-inst` is an **instruction** index
   (`lib/Giri/Giri.cpp:50`, resolved at `Giri.cpp:296-303`), not a source line.

## The degenerate-golden question, sharpened by the drift finding

`llvm-5-criterion-drift-sweep` (`df93296`) established that criterion instruction indices **do**
drift between 3.4 and 5.0.2: `matrix_mult:285` in 3.4 is `matrix_mult:292` in 5.0.2, +7 instructions.

That makes `kmeans-seq`'s clean PASS *weaker* evidence, not stronger. A 19-line golden detected the
+7 drift immediately. A **two-line** golden may well match either side of a drift, because there is
almost nothing in it to disagree about. So "empty diff" does not establish that `main 120` still
points where 3.4's `main 120` pointed — and that is the question.

Settle it the same way the sweep did: dump `main`'s instruction at index 120, name it and its `!dbg`
line, and check whether nearby indices *also* reproduce the two-line golden. If a range of indices
all match, the golden is degenerate and says so; that is a finding about the golden, not the port.

## Decide the pthread variant

Implement exactly one, with the reasoning in this note:

- **Document a CPU-restricted run requirement** (`docker run --cpuset-cpus=…`) — but verify the
  premise first: this only works if `sysconf(_SC_NPROCESSORS_ONLN)` honours the cpuset (glibc answers
  it from `sched_getaffinity` in recent versions, from `/sys` in older ones). Print `num_procs` from
  a restricted run before adopting it. If it still reports 256, this option does not exist.
- **Give kmeans an input large enough** that `num_per_thread > 0` on a many-core host — this changes
  the trace and therefore the slice, so the golden relationship must be re-checked, not assumed.
- **Remove kmeans from `test/auto-tests.txt`** with a written reason and a pointer here.

Editing `ans-*.txt` is not an option.

---

## Definition of done

Part 1:

- [ ] A traced binary that crashes produces `[FAIL]` at the **trace stage** — paste the harness's
      output. Demonstrate under both `EXIT_UNCHECKED=1` and `EXPECTED_EXIT`, with a temporary test-source
      edit that is reverted (show the revert in the diff)
- [ ] Repeat it for a crash that leaves a **usable** trace — `raise(SIGSEGV)` at the very end of
      `main`, after the traced work — so the `[FAIL]` cannot be credited to a later stage failing
- [ ] `test3`'s per-test log still contains `fibonacci(15) is 610` — paste the line
- [ ] Mechanism chosen and argued; if the runtime changed, the consequence for every traced program
      is written down here and in `AGENTS.md`
- [ ] The `EXPECTED_EXIT = 2` / SIGINT collision made visible in the four declaring tests
- [ ] `porting/AgentGuide.md`'s exit-status section true as written — it currently carries a block
      describing this defect and pointing here; replace it with an accurate description of whatever
      the chosen mechanism leaves true

Part 2:

- [ ] `main`'s instruction count in the 5.0.2 build, instruction #120 verbatim with its `!dbg` line,
      and whether `main 402` (pthread) resolves at all — all pasted
- [ ] Index sweep around 120 against the two-line golden, table pasted. State plainly whether the
      golden is degenerate
- [ ] The "criterion line 402 out of range" claim corrected in `kmeans.md` and `SUMMARY.md`
- [ ] The `kmeans-seq.md` exit-code error corrected: it says "Exit code 10 (expected — program
      returns cluster count)", but `kmeans-seq.c` ends in `return 0;` and the Makefile declares no
      `EXPECTED_EXIT`, so a non-zero exit would have failed the trace stage. It exits 0
- [ ] One pthread outcome implemented, reasoning here, consequence in `AGENTS.md`; if the cpuset
      option is chosen, the measured `num_procs` under restriction is pasted as its evidence

Both:

- [ ] Full suite re-run, **count pasted**: expected 21 PASS / 1 FAIL, the failure being
      `matrix_multiply-seq` (standing `FAIL-EXPECTED`, criterion drift — see
      `porting/TestAudit/llvm-5.0.2/matrix_multiply-seq.md`). Any other change is a finding
- [ ] No `ans-*.txt`, no criterion file, and no permanent change to any test's `.c` source
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"). **One image
build and one container for both parts:**

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-final --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-final bash -lc 'source /giri/utils/build.sh'
```

`--cpuset-cpus=0-3` is load-bearing here: it is also the thing Part 2 has to measure. Check free disk
before any `kmeans-pthread` run and `make clean -C /giri/test/kmeans` immediately after.

Rebuild with `make -j$(nproc) -C /giri/build`, not `docker build`. Name the variant explicitly:

```bash
docker exec giri-final make -s -C /giri/test/kmeans TEST_PARALLELISM=seq DEBUGFLAGS=
docker exec giri-final make test -s -C /giri/test/kmeans TEST_PARALLELISM=seq DEBUGFLAGS=
```

If you choose Part 1 mechanism B, traced binaries link `librtgiri.a` statically — every test must be
relinked, so `make clean -C /giri/test` before the suite run or you measure stale executables.

## Traps

- **`128 + n` does not happen here.** That assumption produced Part 1; do not reintroduce it.
- `signal(SIGKILL, …)` at `Tracing.cpp:278` is a no-op — SIGKILL cannot be caught. Pre-existing and
  harmless; do not "fix" it and do not build a test around SIGKILL.
- `test/Makefile.common` scores all 22 tests. Re-run the whole suite after every edit to it.
- Never invoke a test directory's default target (`make -C <dir>` with no target) — `all:` depends on
  `lib:` and rebuilds the CMake tree. `make test` does not.
- `matrix_multiply-seq` is settled (`FAIL-EXPECTED`, criterion drift +7). It must still be the one
  failing test when you are done. Do not reopen it.
- The `bbid` / `lsid` / `prtrace` targets pipe into `view -` (vim) and hang under `docker exec` — run
  `opt … -dump-bbid=true` / `-dump-lsid=true` or `/giri/build/bin/prtrace <file>` directly.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain, as are Giri's own
  `assert()`s (`Release` CMake build). Test programs' `assert()`s do fire — that is how kmeans aborts.
- Evidence goes in the report or this note; `test/_test_logs/` is gitignored scratch that the next
  suite run overwrites.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `test/Makefile.common` — the trace recipe (Part 1)
- `test/UnitTests/test{2,13,14,16}/Makefile` — collision comments only
- `runtime/Giri/Tracing.cpp` — **only** if Part 1 mechanism B is chosen and argued
- `test/auto-tests.txt`, `Dockerfile`, `test/kmeans/Makefile` — only if the Part 2 outcome requires it
- `porting/TestAudit/llvm-5.0.2/{kmeans.md,kmeans-seq.md,SUMMARY.md}`
- `porting/AgentGuide.md`, `AGENTS.md`
- `porting/TaskNotes/Tasks/llvm-5-final-defects.md` (this note — progress log)
- Read-only: `test/**/ans-*.txt`, `test/**/criterion-*.txt`, `lib/**`, `include/**`, `tools/**`,
  `CMakeLists.txt`, `utils/**`

## Blocked by

- ~~llvm-5-harness-residuals~~
- ~~llvm-5-seq-variant-failures~~
- ~~llvm-5-criterion-drift-sweep~~

Run before `llvm-5-port-closeout`, which reconciles the record once these numbers stop moving. Must
not run concurrently with any task that runs the suite.

## Progress log

- 2026-08-14 `2d042e6` — Part 1A-C: Makefile.common now captures stderr, greps for `[GIRI] Abnormal termination`, fails trace stage on crash. Added SIGINT collision comments to test2/13/14/16. Updated AgentGuide.md. TODO 1A/B/C done; next: build container and test crash detection (Part 1D).
- 2026-08-14 `96e80e7` — Part 2C: corrected kmeans-seq.md exit code (0, not 10), kmeans.md/SUMMARY.md criterion description (instruction index, not source line). next: Part 1D container build + Part 2A/B instruction sweep.

## Handoff
- branch `agent/open-code/llvm-5-final-defects`
Refs: `porting/TaskNotes/Tasks/llvm-5-harness-residuals.md`,
`porting/TestAudit/llvm-5.0.2/kmeans.md`, `porting/TestAudit/llvm-5.0.2/kmeans-seq.md`,
`porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/AgentGuide.md`, `test/Makefile.common`,
`runtime/Giri/Tracing.cpp`, `AGENTS.md`
