# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-8.0.0` was cut from the completed `port/llvm-5.0.2` port (base `5527588`) and
carries its honest harness (per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED` in
`test/Makefile.common` + `[GIRI] Abnormal termination` crash detection). The automated
suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports **21 PASS / 0 FAIL** on the
8.0.0 port — the 5.0.2 standing failure (`matrix_multiply-seq`, criterion drift) did
**not** recur: the 5.0.2-era retuned criterion (`matrix_mult 291`, commit `ec0e6b7`)
reproduces the 19-line golden exactly under 8.0.0 codegen (19/19 identical, verified by
fresh clean build + manual slice). Every PASS is against the pristine 3.4 goldens —
`git diff 86f3b8a..HEAD -- 'test/**/ans-*.txt'` is empty. Full history and root causes:
`porting/TestAudit/llvm-8.0.0/SUMMARY.md` → "Suite results across the port".

The 8.0.0 image is `giri-llvm-8` (base **ubuntu:18.04**, gcc 7.5, prebuilt x86_64 LLVM
8.0.0 tools, cmake 3.12.4, `libxml2-dev`). The base-image choice is recorded in the
`Dockerfile` comment: 14.04's gcc 4.8 is below LLVM 8's GCC ≥ 5.1 requirement, and
16.04 (xenial) is no longer served by old-releases.ubuntu.com, so the port used bionic
from the normal archive (no repo redirection needed). LLVM 8 builds with C++11
(measured via `llvm-config --cxxflags`), so the CMake flags stay `-std=c++11`.

Build/test (inside a `giri-llvm-8` container; see `porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-8 .          # from the repo root
docker run -it --rm giri-llvm-8 bash
source /giri/utils/build.sh            # cmake + make + make -C test
```

Source changes for 8.0.0 were minimal (commit `16236bd`):
- Four one-line `#define DEBUG(X) DEBUG_WITH_TYPE(DEBUG_TYPE, X)` compat shims
  (`lib/Giri/Giri.cpp`, `lib/Giri/TraceFile.cpp`,
  `lib/Utility/BasicBlockNumbering.cpp`, `lib/Utility/LoadStoreNumbering.cpp`) —
  LLVM 8's `Debug.h` no longer defines the bare `DEBUG` macro (renamed to
  `LLVM_DEBUG` in 7.0.0); each file already defines `DEBUG_TYPE` and includes
  `Debug.h`, so behavior is preserved (no-op under `NDEBUG`).
- `tools/Tracer/Tracer.cpp`: `WriteBitcodeToFile` takes `const Module &` in 8.0.0
  (2 call sites changed from `M.get()` to `*M`).
- `CallSite` **survives in 8.0.0** (deprecated there, removed in a later release), so
  the three `CallSite` call sites (`TracingNoGiri.cpp`, `TraceFile.cpp`,
  `SourceLineMapping.cpp`) needed no migration.

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum`, `test/Makefile.common`
   lines 45 and 85); behaviorally proven by the 21/21 PASS against the 3.4 goldens.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` byte-identical to the 3.4 base
   (`git diff 86f3b8a..HEAD` empty); `sizeof(Entry)` = 32 on x86_64 LP64, divides the
   4096 page size exactly.
3. **Debug info** — `-g` mandatory in the test compile
   (`CFLAGS += -g -O0 -c -emit-llvm`); `SourceLineMapping` yields the 3.4-era
   `file:line` slices across all 21 passing tests.

**The suite measures the seq variants.** `Dockerfile` sets `TEST_PARALLELISM=seq`, so a
suite result says nothing about a pthread variant unless run by hand. Manual pthread
runs (this port): `pca-pthread` CLEAN (34/34); `matrix_multiply-pthread`
**FAIL-EXPECTED** — criterion instruction drift, 3.4's `matrixmult_map:138` ≠ 8.0.0's
#138 (148 instructions in the function under 8.0.0 codegen); full 1–148 sweep:
**no index reproduces the 60-line golden**, #136 closest (58/60, missing only
lines 102/103); 10 golden lines absent at `:138`, 0 extra (monotonic subset, no
wrong lines). No exact retune exists, so the criterion file was **not** touched —
see the open item in `porting/TestAudit/llvm-8.0.0/matrix_multiply-pthread.md`.
`kmeans-pthread`
**FAIL-HARNESS** — asserts `num_threads == num_procs` (`kmeans-pthread.c:316`) on the
256-CPU host (100 points → 100 threads), caught by the `[GIRI] Abnormal termination`
marker; identical to the 5.0.2 finding (108 GB trace then, 101 GB here).

## Known residuals

The port is functionally closed (21/21 on the honest seq suite). Inherited gaps (never
covered by any LLVM version) are marked [inherited]; regressions (broken by the port)
are [regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `matrix_multiply-pthread` — FAIL-EXPECTED | Final state (not in the automated suite) | Criterion instruction drift (3.4's `matrixmult_map:138` ≠ 8.0.0's #138; 148 instructions under 8.0.0 codegen). Full 1–148 sweep: no index reproduces the 60-line golden; #136 closest (58/60, residual lines 102/103). Golden untouched (pristine 3.4); criterion untouched (no exact retune exists). Evidence: `porting/TestAudit/llvm-8.0.0/matrix_multiply-pthread.md` |
| `kmeans-pthread` — cannot run | [inherited] gap | Asserts on hosts where `sysconf(_SC_NPROCESSORS_ONLN)` exceeds 100 (256 in this container). Harness catches the abort via the `[GIRI] Abnormal termination` marker. Same as 5.0.2 |
| No pthread suite coverage | [inherited] gap | `Dockerfile` pins `TEST_PARALLELISM=seq`. pthread variants are one-off manual measurements, not ongoing coverage |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2 |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite |
| `ensurePostDomFrontierComputed` — memory leak | Acceptable | `DynamicGiri::ensurePostDomFrontierComputed` (`Giri.cpp:67`) `new`s the `PostDominatorTreeWrapperPass` and `PostDominanceFrontier` and frees neither; bounded by `opt` process lifetime, no observable cost. Inherited from 5.0.2 |
| `signal(SIGKILL, …)` — no-op | Harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX; the `signal()` call returns `SIG_ERR`. Inherited from 5.0.2 |

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