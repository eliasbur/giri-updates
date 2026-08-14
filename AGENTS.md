# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

As of this entry (`99423d6`), `port/llvm-5.0.2` has an honest harness with per-test exit
status opt-in (`EXPECTED_EXIT`/`EXIT_UNCHECKED` in `test/Makefile.common`). The suite
reports **21 PASS / 1 FAIL**. The single failure is `matrix_multiply-seq`.

Its segfault is fixed: `PostDominanceFrontier::calculate` held a reference into `Frontiers`
across recursive insertions while `Frontiers` was a `DenseMap`, which invalidates references
when it grows. `Frontiers` is now a `std::map`, matching LLVM 3.4
(`include/Utility/PostDominanceFrontier.h:28`); reverting the change reproduces the 13
`Could not find Control-dep` warnings.

The remaining diff against `ans-inst-seq.txt` (16 extra lines, 1 missing line 97) is
resolved by criterion drift: LLVM 3.4's `matrix_mult:285` is 5.0.2's `matrix_mult:292`, the
`dprintf("\n")` call at source line 97 (both `!dbg !259`), a drift of **+7** instructions
within the output-printing loop. 5.0.2's #285 is now the value-print `fprintf` at line 94
(`!dbg !252`). The golden is exactly reproducible from index 292 (confirmed by sweep of
250–298; index 291, the `load @stdout` argument to the same call, also matches). The 16
extras (15 data-dependence, 1 control-dependence) enter because the value-print reads
`matrix_out`, pulling the computation chain into the slice. The cause of the +7 offset
between LLVM 3.4 and 5.0.2 is unexplained but does not affect the verdict: `FAIL-EXPECTED`.
Measured facts preserved: 298 instructions, #285 = line 94, #224 = line 84 store,
`-trace-cd=false` classifies 15 DD / 1 CD extras.

**The suite measures the seq variants.** `Dockerfile:5` sets `TEST_PARALLELISM=seq` and the
three benchmark Makefiles use `?=`, so a suite result says nothing about a pthread variant
unless that variant was run by hand. This is also the answer to the audit's two "Unresolved
questions": the baseline scored `pca-seq`/`kmeans-seq` (both clean), the audit analysed the
pthread variants. Re-verified by `llvm-5-matrix-multiply-verdict`: `matrix_multiply-pthread`
and `pca-pthread` both pass with empty diffs. `kmeans-pthread` has **not** been re-run since
the pre-`3b26ea6` audit, whose findings (assertion abort at 256 CPUs, 108 GB trace) are its
last measured state. Always name the variant when recording a result.

The original honest-harness run (7 PASS / 15 FAIL) has been resolved: all 15
non-zero-exit UnitTests are inherent program behaviour and now declare their expected
exit codes in their Makefiles. `test9` (uninitialised `sum` → UB) uses `EXIT_UNCHECKED=1`.

**A traced binary never dies by a signal.** Giri's runtime handles the fatal signals and
its handler ends in `exit(signum)` (`runtime/Giri/Tracing.cpp:253-256`), so a segfaulting
traced program exits 11 and an aborting one exits 6 — `128 + n` never appears, and a small
exit status is ambiguous between `main`'s return value and a crash. The reliable marker is
the stderr line `[GIRI] Abnormal termination, signal number <n>`.
`llvm-5-harness-signal-detection` closes this; `porting/AgentGuide.md` documents it.

The port (`llvm-5-port.md`), full-suite audit (`llvm-5-test-audit`), three test defects
(`llvm-5-test-fixes`: PostDominanceFrontier virtual-root recursion, `findAllStoresForLoad`
nestID issue; `llvm-5-seq-variant-failures`: the `DenseMap` reference invalidation above,
plus seq-variant audit reports) and three harness tasks (`llvm-5-harness-honesty`,
`llvm-5-harness-fallout`, `llvm-5-harness-residuals`) are done, as is
`llvm-5-matrix-multiply-verdict` apart from the correction above. Open tasks, in order:
`llvm-5-kmeans`, `llvm-5-harness-signal-detection`, `llvm-5-port-closeout`.
`porting/llvm-releases/5.0.0/api-breakings.yaml` is triaged for only 4 of its 388
entries; finishing it is deliberately deferred.

## Containers — two kinds

There are **two** containers in this workflow:

1. **Agent devcontainer** — the workspace where this agent runs, where source code lives, and where `driver.py` is invoked. This is your shell.
2. **Giri Docker container** — a separate container that builds and tests Giri against a specific LLVM version. Paths prefixed with `/giri/` exist **only inside this container**.

**Never run tests from the devcontainer.** Build and test Giri by running a Giri Docker container, then iterating inside it:

```bash
docker build -t <image-name> .         # full image build (includes LLVM toolchain)
docker run -it --rm <image-name> bash  # iterate inside container
```

Inside the running container:

```bash
source /giri/utils/build.sh   # rebuilds modified parts and runs tests
```

Build output is flat: `build/lib` (`libgiri.so`, `libdgutility.so`, `librtgiri.a`) and `build/bin` (`tracer`, `prtrace`).

For detailed build, test, and debugging commands inside the Giri container, see `porting/AgentGuide.md`.

## Picking up work

Tasks live in `porting/TaskNotes/Tasks/`. Use the `handle-task` skill to:

1. Read the task note
2. Resolve repo/backend/branch with `driver.py resolve`
3. Plan, implement, push, open an MR/PR
4. Finish with `driver.py finish`

Credentials are loaded by the devcontainer from `.env` (gitignored). `driver.py` reads them from environment variables — **you do not need to source `.env` manually** inside the devcontainer. See `.devcontainer/open-code/.env.example` for the expected variable names.

If the target `port/llvm-X` branch doesn't exist, create it from `master` before starting work. See `porting/README.md` for the branch structure.

## Code layout

| Path | Purpose |
|---|---|
| `lib/Giri/TracingNoGiri.cpp` | Instrumentation pass |
| `lib/Giri/Giri.cpp` | Backward-slice computation |
| `lib/Giri/TraceFile.cpp` | (~1500 lines) Trace parser, most subtle file |
| `include/Giri/Runtime.h` | `Entry` struct (trace record format) — ABI boundary |
| `runtime/Giri/Tracing.cpp` | `librtgiri` — C++/pthreads, **no LLVM dependency** |
| `lib/Utility/BasicBlockNumbering.cpp` | BB numbering |
| `lib/Utility/LoadStoreNumbering.cpp` | Load/store numbering |
| `lib/Utility/PostDominatorFrontier.cpp` | Post-dominance frontier |
| `lib/Utility/SourceLineMapping.cpp` | Debug-info -> `file:line` mapping |

## Critical invariants

Three invariants must be preserved during any port. Read the canonical description in `porting/HowItWorks.md` — "Key invariants a port must preserve":

1. **Numbering determinism** — `-bbnum`/`-lsnum` must assign identical IDs in both instrumentation and slicing runs.
2. **`Entry` struct ABI** — `Runtime.h` layout and size-divides-page-size invariant must not change.
3. **Debug info** — `-g` is mandatory; `SourceLineMapping` depends on it.

## Code conventions

- LLVM 3.4-vintage C++ — old pass manager, `OwningPtr` in places, no range-for rewrites.
- Pre-existing filename inconsistency: `include/Utility/PostDominanceFrontier.h` vs `lib/Utility/PostDominatorFrontier.cpp` — not a typo.

## Version-specific docs

- **Agent guide:** `porting/AgentGuide.md` — build/test/debugging commands (run inside Giri container)
- **How Giri works:** `porting/HowItWorks.md` — deep dive into the tracing/slicing pipeline + invariants
- **LLVM API changes:** `porting/llvm-releases/<version>/api-breakings.yaml` — structured API deltas
- **Task notes:** `porting/TaskNotes/Tasks/` — agentic task notes for the `handle-task` skill
- **Task template:** `porting/TaskNotes/Tasks/.task-template.md` — OBS frontmatter schema for new tasks