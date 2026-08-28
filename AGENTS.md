# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-15.0.0` (working branch `agent/jcode/llvm-15.0.0-port`) is the
**new pass manager** port to LLVM 15.0.0, cut from the completed 14.0.0 new-PM
head (`72258e4`). It inherits the entire 9.0.0–14.0.0 change set unchanged
(pass conversion, plugin registration, the harness, and the `Tracer`'s
hand-built `ModuleAnalysisManager`) and re-executes the pipeline on the
**new pass manager** — the forward-compatible path that survives LLVM 16+,
where the legacy PM is actually removed. **The legacy PM is still present in
15.0.0** (it was deprecated in 14 and is removed *after* 14, i.e. in 16):
`opt` retains `--enable-new-pm` and `-enable-new-pm=0` still works; the new PM
is the default. This branch deliberately runs the new-PM path only — no
`-enable-new-pm=0` anywhere; every `opt` invocation runs a `-passes="…"`
pipeline and each Giri library is loaded twice — `-load` (registers the
plugin's `cl::opt` globals *before* `opt` parses the command line:
`-trace-file`, `-slice-file`, `-criterion-*`, `-dump-bbid`, `-mapping-*`) plus
`-load-pass-plugin` (registers the `-passes` pipeline names *after* parsing).

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports
**22 PASS / 0 FAIL** (rc=0) on the 15.0.0 new-PM port: 19 UnitTests
(test1–5, test8–21) plus the three app benchmarks in their seq variant, all
against the pristine 3.4 goldens. A second, independent validation of the
**standalone `tracer`** binary (not `opt`) over the same 22 test cases is also
**22 PASS / 0 FAIL**. `git diff 72258e4..HEAD -- test/` is empty except the
pre-approved harness lines in `test/Makefile.common` and
`test/HelloWorld/Makefile` (the 15.0.0 toolchain parity flags:
`-Xclang -no-opaque-pointers`, `-fPIE`/`-no-pie`, and
`-Wno-error=implicit-function-declaration`); no golden or criterion file
changed. Full history and root causes:
`porting/TestAudit/llvm-15.0.0/SUMMARY.md`.

The 15.0.0 image is `giri-llvm-15` (base **ubuntu:20.04**, prebuilt x86_64
LLVM/Clang **15.0.0** `clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz`
GitHub-Releases tarball at `/usr/local/llvm`, `llvm-config --version` ==
15.0.0). The rhel-8.4 prebuilt is glibc 2.28, so the base image had to move off
ubuntu:18.04 (glibc 2.27) to ubuntu:20.04 (glibc 2.31). CMake 3.31.12 is
pinned. Build/test (inside a `giri15` container; see `porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-15 .          # from the repo root
docker run -it --rm giri-llvm-15 bash
source /giri/utils/build.sh             # cmake + make + make -C test
```

What this port changes on top of the 14.0.0 new-PM head (`72258e4`). The
inherited new-PM machinery (pass conversion, plugin registration, `mergereturn`
parity, the `Tracer`'s hand-built MAM + built-in-analysis registration) is
unchanged; the 15.0.0 work is:

- **Toolchain.** `Dockerfile` → `ubuntu:20.04` + `ENV DEBIAN_FRONTEND=noninteractive`
  (focal's tzdata/apt prompt would otherwise hang an unattended build); CMake
  3.31.12 pinned; `utils/install_llvm.sh` gains a `15.0.0` case (the only
  x86_64-linux prebuilt is the rhel-8.4 tarball, verified against the GitHub
  API). `CMakeLists.txt` floor `cmake_minimum_required` `3.4.3 → 3.5` and
  `find_package(LLVM 15.0 REQUIRED CONFIG)` — both **Giri-side
  forward-compatibility choices, not a direct LLVM-15 mandate on consumers**:
  the 15.0.0 source `CMakeLists.txt` floor is 3.5, and the prebuilt's
  `LLVMConfigVersion.cmake` sets no CMake version floor. (15.0.0 builds with
  **C++14**, not C++17 — `llvm-config --cxxflags` is `-std=c++14`; C++17
  becomes a hard requirement in 16.0.0, and the 15.x toolchain minimums are
  soft and skippable with `-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON`.)
- **`tracer` link shim** (`tools/Tracer/llvm_std_shim.cpp`). The prebuilt
  rhel-8.4 LLVM 15.0.0 libs reference `std::__throw_bad_array_new_length`
  (GLIBCXX 3.4.29); focal's libstdc++ 6.0.28 predates that out-of-line helper.
  A minimal shim (a 3-line function body, `llvm_std_shim.cpp`) supplies it so
  the tracer links and runs.
- **Harness parity** (`test/Makefile.common`, `test/HelloWorld/Makefile`):
  `-Xclang -no-opaque-pointers` (the 15.0.0 clang driver does not expose
  `-opaque-pointers` directly, so opaque pointers are disabled at the front
  end to keep the IR in the explicit-pointer form the passes and 3.4-era
  goldens expect); `-fPIE`/`-no-pie` (the 15.0.0 clang driver defaults to
  `-fPIE`); and `-Wno-error=implicit-function-declaration` (15.0.0's clang
  errors on implicit function declarations; the 3.4-vintage `even.c` relies on
  them).
- **`Tracer` `DataLayout` self-assignment removed**
  (`tools/Tracer/Tracer.cpp`). The inherited line
  `M->setDataLayout(M->getDataLayout())` is a **self-assignment** through the
  hand-written `DataLayout::operator=` (`llvm/IR/DataLayout.h:213`,
  byte-identical in 14.0.0 and 15.0.0): it calls `clear()` and then copies
  members from the (same, now-cleared) source, corrupting the parsed
  alignment/pointer tables (measured: `ABI(i32) 4 → 65536`). This was a
  pre-existing defect in this line, **latent under 14.0.0** (the `opt`-driven
  suite never executes this line and does not verify by default). LLVM 15.0.0
  added a module-verifier check
  (`DataLayout::ParamMaxAlignment = 1 << 14`) that rejects a call argument
  whose ABI alignment exceeds 2^14; the corrupted layout reports absurd
  alignments, so the standalone `tracer`'s trailing `VerifierPass` now fails
  on every pointer-argument call and aborts. Removing the (no-op) line fixes
  the root cause.

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum"`,
   `test/Makefile.common`), so the same `bbnum`/`lsnum` IDs are assigned in
   both runs; behaviorally proven by the 22/22 PASS against the 3.4 goldens.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched by this port
   (`git diff 72258e4..HEAD -- include/Giri/Runtime.h` is empty); re-checked
   in-container: `sizeof(Entry)` = 32 on x86_64 LP64, `4096 % 32 == 0`.
3. **Debug info** — `-g` mandatory in the test compile; `clang -g` on 15.0.0
   emits `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era
   `file:line` slices across all 22 passing tests.

**The suite measures the seq variants.** `Dockerfile` sets
`TEST_PARALLELISM=seq`, so a suite result says nothing about a pthread variant
unless run by hand. The pthread variants were **not** re-measured for 15.0.0
(the automated suite is the parity gate for this branch); their 8.0.0 findings
stand as the last measurements (`porting/TestAudit/llvm-8.0.0/`).

> The **14.0.0 new-PM** port lives on the sibling branch `port/llvm-14.0.0`
> (working branch `agent/jcode/llvm-14-newpm-port`, head `72258e4`, PR #20)
> and also reports 22/22 there; this branch cuts from that head and carries its
> `AGENTS.md` baseline. The **14.0.0 legacy-pass-manager** port
> (`port/llvm-14.0.0-legacypm`, PR #19) keeps `-enable-new-pm=0` because the
> legacy PM still exists in 14.0.0; this branch is the forward-compatible
> variant.

## Known residuals

The port is functionally closed (22/22 on the honest seq suite, plus a 22/22
standalone-`tracer` validation). Inherited gaps (never covered by any LLVM
version) are marked [inherited]; regressions (broken by the port) are
[regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `opt` needs `-load` + `-load-pass-plugin` for each plugin | New-PM-specific, inherent | 15.0.0's new-PM driver discovers passes only via `-load-pass-plugin`; the plugin's `cl::opt` globals still need the plain `-load` (pre-parse dlopen). The harness loads each library twice and documents this. Inherent to 15.0.0 plugin loading, not a bug. |
| `opt -stats` prints nothing (vs 14.0.0's `Statistics are disabled…`) | [inherited] harmless toolchain behavior | The harness passes `-stats`, but the 15.0.0 prebuilt `opt` prints nothing to stderr for it, whereas the 14.0.0 prebuilt `opt` prints `Statistics are disabled.  Build with asserts or with -DLLVM_FORCE_ENABLE_STATS` (44× in the 14.0.0-newpm suite logs; 0× in 15.0.0). Stderr only — never pollutes the `.ll`/trace/IR files and does not change the slice. |
| Pthread variants not re-measured on 15.0.0 | [inherited] gap (suite scope) | `Dockerfile` pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0 audit's. Out of the automated suite, same as 5.0.2/8.0.0/14.0.0 |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0/14.0.0 |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite (HelloWorld's Makefile was updated to the new-PM harness so it runs by hand — verified on 15.0.0: slice lines `8 10`) |
| `Tracer` `DataLayout` self-assignment removed | Fixed / [inherited] benign root cause | The inherited line was latent under 14.0.0; 15.0.0's verifier `ParamMaxAlignment = 1<<14` check exposed it. Removed (the no-op it is); the standalone `tracer` no longer aborts (22/22). |
| `ensurePostDomFrontierComputed` — bounded per-function allocation | Replaced / [inherited] benign | `DynamicGiri` builds the `PostDominatorTree` inline (public `Function&` ctor) then runs `PostDominanceFrontier::computeFrontiers`; the per-function tree/frontier is still not freed — same `opt`-process-lifetime bound as the 14.0.0 port, no observable cost. |
| `signal(SIGKILL, …)` — no-op | [inherited] harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX. Inherited from 5.0.2/8.0.0/14.0.0 |

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
