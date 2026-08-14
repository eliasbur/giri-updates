# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

As of this entry (`df93296`), `port/llvm-5.0.2` has an honest harness with per-test exit
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
and `pca-pthread` both pass with empty diffs. `kmeans-pthread` aborts on hosts with many CPUs
(`sysconf(_SC_NPROCESSORS_ONLN)` = 256 inside the container; `kmeans-pthread.c:316` asserts
`num_threads == num_procs`, but with 100 points only 100 threads are created). The harness now
correctly catches this crash at the trace stage via the `[GIRI] Abnormal termination` marker
(verified in `llvm-5-final-defects`). A cpuset-restricted run (`--cpuset-cpus=0-3`) would be
needed for a many-cpu host but is unavailable on the current Ubuntu 14.04 container runtime.
Always name the variant when recording a result.

`kmeans-seq`'s golden (`ans-inst-seq.txt`) is only two lines, which raised the question of whether
its clean PASS was evidence of anything. It is: a sweep of instruction indices 114–126 in `main`
found index **120** to be the only one reproducing `[222, 276]` (#120 is
`call @dump_matrix`, `kmeans-seq.c:276`; `main` has 165 instructions). The golden is **not**
degenerate, and it did not drift the way `matrix_mult`'s did.

The original honest-harness run (7 PASS / 15 FAIL) has been resolved: all 15
non-zero-exit UnitTests are inherent program behaviour and now declare their expected
exit codes in their Makefiles. `test9` (uninitialised `sum` → UB) uses `EXIT_UNCHECKED=1`.

**A traced binary never dies by a signal.** Giri's runtime handles the fatal signals and
its handler ends in `exit(signum)` (`runtime/Giri/Tracing.cpp:253-256`), so a segfaulting
traced program exits 11 and an aborting one exits 6 — `128 + n` never appears, and a small
exit status is ambiguous between `main`'s return value and a crash. The trace recipe
(`test/Makefile.common`) now detects this: it captures stderr, greps for the marker
`[GIRI] Abnormal termination`, and fails the trace stage if found — covering both
`EXIT_UNCHECKED` and `EXPECTED_EXIT` cases. `porting/AgentGuide.md` documents the mechanism.

Done: the port (`llvm-5-port`), the full-suite audit (`llvm-5-test-audit`), three code
defects (`llvm-5-test-fixes` — PostDominanceFrontier virtual-root recursion and the
`findAllStoresForLoad` nestID collision; `llvm-5-seq-variant-failures` — the `DenseMap`
reference invalidation above), three harness tasks (`llvm-5-harness-honesty`,
`llvm-5-harness-fallout`, `llvm-5-harness-residuals`), the two-step
`matrix_multiply-seq` verdict (`llvm-5-matrix-multiply-verdict`,
`llvm-5-criterion-drift-sweep`) and `llvm-5-final-defects` (harness crash detection + the
kmeans settlement; it superseded the former `llvm-5-kmeans` and
`llvm-5-harness-signal-detection`).

Open: **`llvm-5-closeout-corrections`** — a short follow-up. A head-agent review of `83bf08d` found
four closeout Definition-of-done boxes ticked against work that was not done, one of which put a
false row into the `## Known residuals` register below. No container needed. Until it lands, treat
the register's last row and `SUMMARY.md`'s per-test verdict table as unreliable.

Done: **`llvm-5-port-closeout`** — It verified the three critical invariants
(see below), reconciled the notes and reports, fixed one defect inherited from `llvm-5-final-defects`
(`test/Makefile.common` `$_tmperr` quoting, 8 sites, replaced by per-test `*.trace.err` file),
and produced the Known residuals register below.
`porting/llvm-releases/5.0.0/api-breakings.yaml` is triaged for only 4 of its 388
entries; finishing it is deliberately deferred.

The three critical invariants are verified (2026-08-14, `llvm-5-port-closeout`):
1. **Numbering determinism** — `-bbnum`/`-lsnum` assign identical IDs across instrumentation and
   slicing runs, and across consecutive runs. Verified for test2 (single-file, 5 BBs, 20 LS points)
   and test16 (multi-file via `llvm-link`, 8 BBs); repeat runs agree. Both pipelines query IDs from
   the same `.all.bc` using `QueryBasicBlockNumbers`/`QueryLoadStoreNumbers` pass, which iterate
   `Module::iterator` / `Function::iterator` / `BasicBlock::iterator` deterministically.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` is byte-identical to `master` on this branch
   (`git diff` empty). `sizeof(Entry)` = 32 on x86_64 LP64; divides sysconf(_SC_PAGESIZE) = 4096
   exactly (128 entries per page).
3. **Debug info** — `-srcline-mapping` produces populated `file:line` mappings matching source.
   Verified on test2: `func` at `ifelse.c:4–5`, `main` at `ifelse.c:8–14`, with `NIL` for
   instructions lacking debug info (allocas, unannotated stores).

## Known residuals

The port is functionally closed. These are the reasons a future agent might reopen work, what is
acceptable, and where the evidence lives. Inherited gaps (never covered by any LLVM version) are
marked [inherited]; regressions (broken by the port) are [regression].

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `matrix_multiply-seq` — 1 FAIL | Acceptable standing failure | Criterion drift: LLVM 3.4's `matrix_mult:285` is 5.0.2's `matrix_mult:292` (+7 offset within output-printing loop). 16 extra lines are traceable data-dependence from the drift. Verdict: `FAIL-EXPECTED`. Evidence: `llvm-5-criterion-drift-sweep` (`df93296`); AGENTS.md Current state |
| `kmeans-pthread` — cannot run | [inherited] gap | Asserts on hosts where `sysconf(_SC_NPROCESSORS_ONLN)` exceeds 100. Container reports 256 CPUs; harness now catches the abort at trace stage. A cpuset-restricted run (`--cpuset-cpus=0-3`) would be needed but is unavailable on the current Ubuntu 14.04 container runtime. Harness correctly reports `[FAIL]` with `[GIRI] Abnormal termination` marker |
| `kmeans-seq` — 2-line golden | Verified not degenerate | Sweep of indices 114–126 in `main` found index **120** (`call @dump_matrix`, `kmeans-seq.c:276`) as the only one reproducing `[222, 276]`. Golden did not drift unlike `matrix_mult`. Evidence: `llvm-5-final-defects` (`e194151`) |
| No pthread suite coverage | [inherited] gap | `Dockerfile:5` pins `TEST_PARALLELISM=seq`. `matrix_multiply-pthread` and `pca-pthread` checked once by hand in `llvm-5-matrix-multiply-verdict` — both clean. Not ongoing coverage; one-off measurement at that commit |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`. test6/test7 require interactive terminal setup for signals; test22 needs `-lm` linker flag. Decision recorded in `test/auto-tests.txt` header |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on **any** LLVM version. Not wired into the suite. Never verifiable |
| `api-breakings.yaml` — 384/388 untriaged | Deferred by decision | The port demonstrably triaged and addressed the relevant entries as it fixed compiler errors. Finishing the remaining 384 (mostly header moves, unused API changes) is deferred. 4 entries carry `relevance: "affected"` / `status: "addressed"` |
| `ensurePostDomFrontierComputed` — memory leak | Acceptable | `DynamicGiri::ensurePostDomFrontierComputed` (`Giri.cpp:67`) `new`s a `PostDominatorTreeWrapperPass` per function and never frees it. The pass's process (`opt`) exits immediately after, so the leak is bounded by process lifetime and has no observable cost |
| `properlyDominates` overload change | Verified acceptable | The port changed `DT.properlyDominates(Node, DT[*CDFI])` (DomTreeNode overload) to `DT.properlyDominates(Node->getBlock(), *CDFI)` (BasicBlock overload). The 21-test suite exercising the repaired post-dominator path is the evidence. No divergence observed |
| `signal(SIGKILL, …)` — no-op | Harmless | `runtime/Giri/Tracing.cpp:278`. SIGKILL cannot be caught or ignored per POSIX. The `signal()` call is a no-op (returns `SIG_ERR`). Harmless: the handler for other signals already ensures crash cleanup |
| `signal` handlers reinstall on every trace entry | Acceptable cost | `recordInit` in `Tracing.cpp` installs handlers eagerly. Re-entry during tracing would reinstall, which is correct but slightly wasteful. Not a defect; the handlers are idempotent |

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