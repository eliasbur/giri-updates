# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-12.0.0` is the **legacy pass manager** port to LLVM 12.0.0, cut from the
completed 8.0.0 legacy-PM port head `224bdfb`. It sits on the legacy-PM line
5.0.2 → 8.0.0 → **12.0.0** → 14.0.0-legacypm (PR #19): 12.0.0 uses the legacy PM by
default (no `-enable-new-pm` flag anywhere) and typed pointers by default
(pre-opaque-pointer era), so the 8.0.0 harness needs **zero** changes —
`git diff 224bdfb..HEAD -- test/` **and** `-- include/Giri/Runtime.h` are **both
empty** (the strongest clause: no harness, golden, or criterion file was touched).

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports
**22 PASS / 0 FAIL** (rc=0) on the 12.0.0 port: 19 UnitTests (test1–5, test8–21)
plus the three app benchmarks in their seq variant, all against the pristine 3.4
goldens. A standalone `tracer`/`prtrace` validation over the same 22 cases is
**22 PASS / 0 FAIL** in its validatable scope (the `tracer` instrument stage +
`prtrace` decode; the standalone slice mode is a pre-existing legacy-PM-line
limitation — see Known residuals). Full history and root causes:
`porting/TestAudit/llvm-12.0.0/SUMMARY.md` → "Suite results across the port".

The 12.0.0 image is `giri-llvm-12` (base **ubuntu:20.04**, prebuilt x86_64
LLVM/Clang 12.0.0 GitHub-Releases tarball
`clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz` at `/usr/local/llvm`,
`llvm-config --version` == 12.0.0, cmake 3.12.4 pin). **Documented deviation:** the
`llvmorg-12.0.0` GitHub release ships no x86_64 ubuntu-18.04 asset (x86_64 assets:
ubuntu-16.04, ubuntu-20.04, sles12.4), so the 8/14 images' 18.04 convention cannot
apply; the base moves to 20.04 to match the tarball (recorded in the `Dockerfile`
comment). `llvm-config --cxxflags` = `-std=c++14 …` and the host gcc 9.4.0 default
(`gnu++14`) matches, so **no forced `CMAKE_CXX_STANDARD` pin** is needed (16.0.0
needed one because its headers are C++17); the 8.0.0 CMake flags stay untouched.
`ldd` on `opt`/`clang` is clean on 20.04 (no link shim: the prebuilt's max GLIBCXX
symbol is satisfied by 20.04's libstdc++, and
`std::__throw_bad_array_new_length` is not referenced by any 12.0.0 static lib).

Build/test (inside a `giri-llvm-12` container; see `porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-12 .         # from the repo root
docker run -it --rm giri-llvm-12 bash
source /giri/utils/build.sh             # cmake + make + make -C test
```

Source changes for 12.0.0 (commit `f6b39bf`), each root-caused against the 12.0.0
prebuilt and applied only because the 12.0.0 compiler demands it:
- **`FunctionCallee` (9.0.0)** — `Module::getOrInsertFunction(...)` returns
  `FunctionCallee`, not `Function*`: `cast<Function>(...getCallee())` in
  `include/Utility/Utils.h` (2 sites) and `lib/Giri/TracingNoGiri.cpp` (1).
- **`CallSite` removal (11.0.0)** — `lib/Giri/TraceFile.cpp` casts `const CallBase *`
  and uses `arg_size()`/`getArgOperand(i)`; `lib/Giri/TracingNoGiri.cpp` +
  `lib/Utility/SourceLineMapping.cpp` use `CallBase::getCalledOperand()`; three
  `llvm/IR/CallSite.h` includes dropped. The 8.0.0 `DEBUG(X)` shim **stays**
  (12.0.0 `Debug.h` still has no bare `DEBUG`, only `DEBUG_WITH_TYPE`).
- **`BasicBlockPass` deleted (10.0.0)** — `TracingNoGiri` (the tree's one
  `BasicBlockPass`) became a `FunctionPass` with a `runOnFunction` loop calling
  `runOnBasicBlock` per BB (`include/Giri/Giri.h`, `lib/Giri/TracingNoGiri.cpp`).
- **`tracer` link** — the 12.0.0 prebuilt CMake package sets no
  `LLVM_COMPONENT_LIBS`, so `llvm_map_components_to_libnames(all)` expands empty and
  `tracer` fails to link; replaced with `llvm-config --libfiles all` +
  `--system-libs` (`tools/Tracer/CMakeLists.txt`). The same fix the
  14.0.0-legacypm port made.

Not needed on 12.0.0 (recorded in the task note): the `#include <map>` contingency
(compiles without it), the `sys::fs::F_*`→`OF_*` rename (13.0.0), and the 14.0.0-era
DEBUG_TYPE/STATISTIC and `Twine` fixes.

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum`,
   `test/Makefile.common` lines 45 and 85); two `-bbnum -query-bbnum` runs on the
   same `.bc` are byte-identical; behaviorally proven by the 22/22 PASS.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched
   (`git diff 224bdfb..HEAD -- include/Giri/Runtime.h` empty); `sizeof(Entry)` = 32
   on x86_64 LP64, divides the 4096 page size; every fresh `.trace` `% 32 == 0`;
   `prtrace` decodes all 22 traces ending in the `End` record.
3. **Debug info** — `-g` mandatory in the test compile
   (`CFLAGS += -g -O0 -c -emit-llvm`); `clang -g` on 12.0.0 emits the DWARF DI
   metadata; `SourceLineMapping` yields the 3.4-era `file:line` slices across all 22
   passing tests.

**The suite measures the seq variants.** `Dockerfile` sets
`TEST_PARALLELISM=seq`, so a suite result says nothing about a pthread variant
unless run by hand. The pthread variants were **not** re-measured for 12.0.0 (the
automated suite is the parity gate for this branch); the 8.0.0 audit's pthread
findings (`matrix_multiply-pthread` FAIL-EXPECTED criterion drift,
`kmeans-pthread` FAIL-HARNESS on the 256-CPU host) stand as the last measurements.

> The **14.0.0-legacypm** port (`port/llvm-14.0.0-legacypm`, PR #19) is the forward
> successor on this line and also reports 22/22; it keeps `-enable-new-pm=0` because
> the legacy PM still exists in 14.0.0. The **16.0.0** new-PM port
> (`port/llvm-16.0.0`, PR #23) is the parallel new-PM line (the forward-compatible
> path that survives LLVM 17+, where the legacy PM is removed) and reports 22/22
> there.

## Known residuals

The port is functionally closed (22/22 on the honest seq suite, plus a 22/22
standalone-`tracer` instrument/`prtrace` validation). Inherited gaps (never covered
by any LLVM version) are marked [inherited]; regressions (broken by the port) are
[regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `matrix_multiply-pthread` — FAIL-EXPECTED | [inherited] (not in the automated suite) | Inherited from 8.0.0: criterion instruction drift (3.4's `matrixmult_map:138` ≠ 8.0.0's #138; **153** instructions under 8.0.0 codegen; N=136 reproduces the 60-line golden). Not re-measured on 12.0.0. Evidence: `porting/TestAudit/llvm-8.0.0/matrix_multiply-pthread.md` |
| `kmeans-pthread` — cannot run | [inherited] gap | Asserts on hosts where `_SC_NPROCESSORS_ONLN` > 100 (256 in this container). Same as 5.0.2/8.0.0 |
| No pthread suite coverage | [inherited] gap | `Dockerfile` pins `TEST_PARALLELISM=seq`. Last pthread measurements: the 8.0.0 audit |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0 |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite |
| `opt -stats` prints nothing | [inherited] harmless toolchain behavior | The 12.0.0 prebuilt `opt` prints nothing to stderr for `-stats` (0× across the suite logs). Stderr-only; never pollutes the `.ll`/trace/slice files or the slice. |
| Standalone `tracer` slice mode SIGBUSes | [inherited] pre-existing legacy-PM-line limitation | The 3.4-vintage `tools/Tracer/Tracer.cpp` has a fixed `main()` pipeline that never applies `-bbnum -lsnum`, so its slice mode (`-dgiri`/`DynamicGiri`) reads empty numbering maps and `findPreviousID` SIGBUSes. Untouched by this port (`git diff 224bdfb..HEAD -- tools/Tracer/Tracer.cpp` empty); the 8.0.0→14.0.0-legacypm delta made no `Tracer.cpp` pipeline change either. The standalone **instrument** stage + `prtrace` are validated (22/22); the slice *result* is validated via `opt` in the suite. |
| `ensurePostDomFrontierComputed` — memory leak | [inherited] benign | `DynamicGiri` `new`s the `PostDominatorTree`/`PostDominanceFrontier` and frees neither; bounded by `opt` process lifetime, no observable cost. Inherited from 5.0.2/8.0.0 |
| `signal(SIGKILL, …)` — no-op | [inherited] harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX. Inherited from 5.0.2/8.0.0 |

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
