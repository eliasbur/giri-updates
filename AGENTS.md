# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

As of `3f54fb1` (2026-08-12), the test harness (`llvm-5-harness-honesty`) has been made honest:
it no longer suppresses non-zero exits from traced binaries and retains per-test stage logs.
Under the honest harness, `port/llvm-5.0.2` reports 7 PASS / 15 FAIL (vs the previously
recorded 21 PASS / 1 FAIL). All 15 newly-failing UnitTests fail at trace generation because
the traced binary exits non-zero — these were previously hidden by error suppression in the
Makefiles. The 7 that do pass (test5, test17, test20, test21, matrix_multiply, pca, kmeans)
complete the full trace/slice/diff pipeline successfully. The 15 failures require diagnosis
of whether the non-zero exits are inherent program behavior under instrumentation.

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