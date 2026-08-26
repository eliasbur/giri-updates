# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-14.0.0` (working branch `agent/jcode/llvm-14-newpm-port`) is the
**new pass manager** port to LLVM 14.0.0, cut from the completed
`port/llvm-14.0.0-legacypm` head (`fba2565`, legacy-PM head `224bdfb`). It
re-executes the entire Giri pipeline on the **new pass manager** — the
forward-compatible path that survives LLVM 15+, where the legacy PM is gone
(deprecated in 14, "removed after LLVM 14"). No `-enable-new-pm=0` anywhere;
every `opt` invocation runs a `-passes="…"` pipeline and each Giri library is
loaded twice — `-load` (registers the plugin's `cl::opt` globals *before*
`opt` parses the command line: `-trace-file`, `-slice-file`, `-criterion-*`,
`-dump-bbid`, `-mapping-*`) plus `-load-pass-plugin` (registers the `-passes`
pipeline names *after* parsing).

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports
**22 PASS / 0 FAIL** (rc=0) on the 14.0.0 new-PM port: 19 UnitTests
(test1–5, test8–21) plus the three app benchmarks in their seq variant, all
against the pristine 3.4 goldens. `git diff fba2565..HEAD -- test/` is empty
except the pre-approved new-PM harness lines in `test/Makefile.common` and
`test/HelloWorld/Makefile` (the `-enable-new-pm=0` + legacy `-<passname>`
flags replaced by `-load` + `-load-pass-plugin` + `-passes="…"`); no golden or
criterion file changed. Full history and root causes:
`porting/TestAudit/llvm-14.0.0-newpm/SUMMARY.md` → "Suite results across the
port".

The 14.0.0 image is `giri-llvm-14` (base **ubuntu:18.04**, prebuilt x86_64
LLVM/Clang **14.0.0** GitHub-Releases tarball, `llvm-config --version` ==
14.0.0). Build/test (inside a `giri-llvm-14` container; see
`porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-14 .          # from the repo root
docker run -it --rm giri-llvm-14 bash
source /giri/utils/build.sh             # cmake + make + make -C test
```

What this port changes on top of the legacy-PM port (the 8→14 C++ API fixes,
toolchain, and Dockerfile are inherited unchanged):

- **Pass conversion.** All Giri/Utility passes/analyses converted from
  legacy-PM classes (`PassInfoMixin` + `getAnalysisUsage` / `RegisterPass`) to
  new-PM `llvm::Pass` / `llvm::Analysis` classes with `run(...)`; pass logic is
  byte-identical (only the plumbing changed). `TracingNoGiri` (the former
  `BasicBlockPass`, deleted in 10.0.0) is now a new-PM **module pass**:
  `run(Module&, MAM)` does the module-level init once, then loops functions and,
  per function, basic blocks, driving the renamed `instrument`/
  `runOnBasicBlock` per-BB logic (per-BB order and insertion order preserved).
  `DynamicGiri` is a new-PM **module analysis** (`AnalysisKey Key`), fetched by
  the `dgiri`/`test-giri` module passes via `MAM.getResult<DynamicGiri>(M)`; its
  `ensurePostDomFrontierComputed` builds the `PostDominatorTree` inline (public
  `Function&` ctor) then runs `PostDominanceFrontier::computeFrontiers`.
  `QueryBasicBlockNumbers`/`QueryLoadStoreNumbers` are new-PM **module
  analyses** whose result object carries the ID maps; the no-op `bbnum`/
  `lsnum`/`remove-*` passes force those lazy analyses
  (`MAM.getResult<Query…>(M)`), preserving the legacy `getAnalysis`-triggered
  numbering behavior. `PostDominanceFrontierAnalysis` (function analysis) takes
  `FAM.getResult<PostDominatorTreeAnalysis>(F)` — `PostDominatorTreeAnalysis`
  (the analysis, `Result = PostDominatorTree`), not the `PostDominatorTree`
  data structure.
- **Plugin registration.** `lib/Utility/UtilityPassPlugin.cpp` (bbnum,
  remove-bbnum, lsnum, remove-lsnum, postdomfrontier, countsrc,
  srcline-mapping + the Query* module analyses + the PostDominanceFrontier
  function analysis) and `lib/Giri/GiriPassPlugin.cpp` (trace-giri, dgiri,
  test-giri + DynamicGiri + re-registered Query* so libgiri works standalone)
  both export `llvmGetPassPluginInfo`. `libgiri.so` → `libdgutility.so` via
  NEEDED; a spike verified cross-library analysis `getResult` sees a single
  analysis instance.
- **`mergereturn` parity.** No test-only `MergeReturn` was needed: in 14.0.0 the
  legacy and new-PM `mergereturn` are the same transform (`UnifyFunctionExitNodes`),
  spike-verified **byte-identical** IR. The harness wraps it in an explicit
  `function(mergereturn)` sub-pipeline (it must be first to fix the top-level
  element type); every Giri pass is a module pass at the top level.
- **`Tracer`.** Builds its instrumentation pipeline programmatically with the
  new PM (`ModuleAnalysisManager` + `PassBuilder`); `WriteBitcodeToFile` is void
  in 14.0.0.

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum"`,
   `test/Makefile.common`), so the same `bbnum`/`lsnum` IDs are assigned in both
   runs; behaviorally proven by the 22/22 PASS against the 3.4 goldens.
   `mergereturn` parity (byte-identical IR, legacy vs new-PM) spike-verified
   separately.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched by this port
   (`git diff fba2565..HEAD -- include/Giri/Runtime.h` is empty); re-checked
   in-container: `sizeof(Entry)` = 32 on x86_64 LP64, `4096 % 32 == 0`.
3. **Debug info** — `-g` mandatory in the test compile; `clang -g` on 14.0.0 emits
   `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era `file:line`
   slices across all 22 passing tests.

**The suite measures the seq variants.** `Dockerfile` sets `TEST_PARALLELISM=seq`, so a
suite result says nothing about a pthread variant unless run by hand. The pthread variants
were **not** re-measured for 14.0.0 (the automated suite is the parity gate for this
branch); their 8.0.0 findings stand as the last measurements
(`porting/TestAudit/llvm-8.0.0/`).

> The **legacy-pass-manager** port lives on the sibling branch
> `port/llvm-14.0.0-legacypm` (working branch `agent/open-code/llvm-14-legacypm`,
> PR #19) and also reports 22/22 there; its `AGENTS.md` copy documents the
> 8→14 API fixes in detail. That branch keeps `-enable-new-pm=0` because the
> legacy PM still exists in 14.0.0; this branch is the forward-compatible
> variant.

## Known residuals

The legacy-PM port is functionally closed (22/22 on the honest seq suite). Inherited gaps
(never covered by any LLVM version) are marked [inherited]; regressions (broken by the
port) are [regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `opt` needs `-load` + `-load-pass-plugin` for each plugin | New-PM-specific, inherent | 14.0.0's new-PM driver discovers passes only via `-load-pass-plugin`; the plugin's `cl::opt` globals still need the plain `-load` (pre-parse dlopen). The harness loads each library twice and documents this. Inherent to 14.0.0 plugin loading, not a bug. |
| Pthread variants not re-measured on 14.0.0 | [inherited] gap (suite scope) | `Dockerfile` pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0 audit's. Out of the automated suite, same as 5.0.2/8.0.0/legacy-PM |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0/legacy-PM |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite (HelloWorld's Makefile was updated to the new-PM harness so it runs by hand) |
| `ensurePostDomFrontierComputed` — bounded per-function allocation | Replaced / [inherited] benign | The legacy lazy-wrapper hack (`new PostDominatorTreeWrapperPass`, freed by neither) is replaced: `DynamicGiri` builds the `PostDominatorTree` inline (public `Function&` ctor, spike-verified) then runs `PostDominanceFrontier::computeFrontier`; the per-function tree/frontier is still not freed — same `opt`-process-lifetime bound as the legacy port, no observable cost |
| `signal(SIGKILL, …)` — no-op | [inherited] harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX. Inherited from 5.0.2/8.0.0/legacy-PM |

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