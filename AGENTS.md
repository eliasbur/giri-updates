# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-14.0.0-legacypm` (working branch `agent/open-code/llvm-14-legacypm`) is the
**legacy pass manager** port to LLVM 14.0.0, cut from the completed `port/llvm-8.0.0`
head (`224bdfb`). It carries the honest harness (per-test `EXPECTED_EXIT`/`EXIT_UNCHECKED`
in `test/Makefile.common` + `[GIRI] Abnormal termination` crash detection). Every `opt`
invocation runs `-enable-new-pm=0` (legacy PM) — the execution path for this branch, kept
while the legacy PM still exists in 14.0.0 (deprecated in 14, "removed after LLVM 14";
the forward-compatible new-PM port is a separate target branch `port/llvm-14.0.0`).

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports **22 PASS /
0 FAIL** on the 14.0.0 legacy-PM port: 19 UnitTests (test1–5, test8–21) plus the three
app benchmarks in their seq variant, all against the pristine 3.4 goldens
(`git diff 224bdfb..HEAD -- test/` is empty except the pre-approved `-enable-new-pm=0`
harness lines in `test/Makefile.common` and `test/HelloWorld/Makefile`). Full history and
root causes: `porting/TestAudit/llvm-14.0.0-legacypm/SUMMARY.md` → "Suite results across
the port".

The 14.0.0 image is `giri-llvm-14` (base **ubuntu:18.04**, prebuilt x86_64 LLVM/Clang
**14.0.0** GitHub-Releases tarball, `llvm-config --version` == 14.0.0). Build/test (inside
a `giri-llvm-14` container; see `porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-14 .          # from the repo root
docker run -it --rm giri-llvm-14 bash
source /giri/utils/build.sh             # cmake + make + make -C test
```

Source changes for 8→14 (commits `306d34f`, `74b870f`):
- `FunctionCallee` (9.0.0): `Module::getOrInsertFunction` returns `FunctionCallee`, so
  the two sites use `cast<Function>(M.getOrInsertFunction(...).getCallee())`
  (`include/Utility/Utils.h`, `lib/Giri/TracingNoGiri.cpp`).
- `CallSite` removal: `lib/Giri/TraceFile.cpp` now casts to `const CallBase *`
  (`CallBase` lives in `llvm/IR/InstrTypes.h`) and uses `CS->arg_size()` /
  `getArgOperand(i)`; `CallBase::getCalledOperand()` replaces the removed
  `getCalledValue()` (`lib/Utility/SourceLineMapping.cpp`, `lib/Giri/TracingNoGiri.cpp`).
- `TracingNoGiri` converted from the removed legacy `BasicBlockPass` (10.0.0) to
  `FunctionPass`; the per-basic-block behavior is preserved by a `runOnFunction` loop that
  calls `runOnBasicBlock` for each BB (`include/Giri/Giri.h`, `lib/Giri/TracingNoGiri.cpp`).
- `sys::fs::F_None`/`F_Append` → `OF_None`/`OF_Append` (13.0.0): `Giri.cpp`,
  `SourceLineMapping.cpp`, `tools/Tracer/Tracer.cpp`.
- `DEBUG_TYPE` is defined **after** the includes (14.0.0 headers such as
  `GenericDomTreeConstruction.h` `#undef DEBUG_TYPE`); the bare `DEBUG(X)` macro is
  re-`#define`d via `DEBUG_WITH_TYPE(DEBUG_TYPE, X)` in the two files that use it
  (`Giri.cpp`, `TraceFile.cpp`). (The earlier `STATISTIC_WITH_TYPE` shim, commit `f0f0f9c`,
  was reverted by this.)
- `tools/Tracer/CMakeLists.txt`: the prebuilt LLVM 14.0.0 CMake package does not populate
  the `LLVM_COMPONENT_LIBS` property (`llvm_map_components_to_libnames` expands empty) and
  does not add the LLVM lib dir to the search path, so `tracer` links the
  dependency-ordered static libs by absolute path via `llvm-config --libfiles all` plus
  `--system-libs` (`-lrt -ldl -lpthread -lm`).

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum`, `test/Makefile.common`
   lines 42–48 and 82–88); behaviorally proven by the 22/22 PASS against the 3.4 goldens.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched by this port; re-checked
   in-container: `sizeof(Entry)` = 32 on x86_64 LP64, `4096 % 32 == 0`.
3. **Debug info** — `-g` mandatory in the test compile; `clang -g` on 14.0.0 emits
   `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era `file:line` slices
   across all 22 passing tests.

**The suite measures the seq variants.** `Dockerfile` sets `TEST_PARALLELISM=seq`, so a
suite result says nothing about a pthread variant unless run by hand. The pthread variants
were **not** re-measured for 14.0.0 (the automated suite is the parity gate for this
branch); their 8.0.0 findings stand as the last measurements
(`porting/TestAudit/llvm-8.0.0/`).

## Known residuals

The legacy-PM port is functionally closed (22/22 on the honest seq suite). Inherited gaps
(never covered by any LLVM version) are marked [inherited]; regressions (broken by the
port) are [regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| Legacy pass manager deprecated in 14.0.0 | Final state for **this** branch (legacy PM) | Release notes: "using the legacy pass manager … is deprecated and will be removed after LLVM 14." This branch intentionally keeps the legacy PM (`-enable-new-pm=0` everywhere). The forward-compatible new-PM port is a separate branch `port/llvm-14.0.0` (includes test-only passes/analyses for full test parity) |
| Pthread variants not re-measured on 14.0.0 | [inherited] gap (suite scope) | `Dockerfile` pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0 audit's (`matrix_multiply-pthread` FAIL-EXPECTED at the shipped `:138`, `kmeans-pthread` FAIL-HARNESS on the 256-CPU host). Out of the automated suite, same as 5.0.2/8.0.0 |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0 |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite (HelloWorld's Makefile only gained the `-enable-new-pm=0` flag) |
| `ensurePostDomFrontierComputed` — memory leak | Acceptable | `DynamicGiri::ensurePostDomFrontierComputed` (`Giri.cpp`) `new`s the `PostDominatorTreeWrapperPass` and `PostDominanceFrontier` and frees neither; bounded by `opt` process lifetime, no observable cost. Inherited from 5.0.2/8.0.0 |
| `signal(SIGKILL, …)` — no-op | Harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX; the `signal()` call returns `SIG_ERR`. Inherited from 5.0.2/8.0.0 |

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