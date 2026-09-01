# AGENTS.md

This repository is a fork of the original Giri project, an implementation of the dynamic backwards code slicing algorithm for the LLVM compiler.
The issue addressed by this repository is that the original Giri program is targeted towards LLVM version 3.4 and works only if itself, as well as the source code that it is run on, is compiled using tools from the LLVM suite of this exact version.
The overall task of this repository is to provide ports of the program for some versions X, present in associated git branches `port/llvm-X`.

Every version branch (`port/llvm-*`) carries a copy of this file and may append a `## Current state` section at the top with branch-specific status.

## Current state

`port/llvm-16.0.0` (working branch `agent/jcode/llvm-16.0.0-port`) is the
**new pass manager** port to LLVM 16.0.0, cut from the completed 15.0.0 new-PM
head (`63b02e2`, PR #21). It re-executes the entire Giri pipeline on the
**new pass manager** — the forward-compatible path that survives LLVM 17+,
where the legacy PM is gone (deprecated in 14, "removed after LLVM 14"; the
legacy PM is **removed in 16**). No `-enable-new-pm=0` anywhere; every `opt`
invocation runs a `-passes="…"` pipeline and each Giri library is loaded twice
— `-load` (registers the plugin's `cl::opt` globals *before* `opt` parses the
command line: `-trace-file`, `-slice-file`, `-criterion-*`, `-dump-bbid`,
`-mapping-*`) plus `-load-pass-plugin` (registers the `-passes` pipeline names
*after* parsing).

The automated suite (`make -C /giri/test`, `TEST_PARALLELISM=seq`) reports
**22 PASS / 0 FAIL** (rc=0) on the 16.0.0 new-PM port: 19 UnitTests
(test1–5, test8–21) plus the three app benchmarks in their seq variant, all
against the pristine 3.4 goldens. A second, independent validation of the
**standalone `tracer`** binary (not `opt`) over the same 22 test cases is also
**22 PASS / 0 FAIL** (the 15.0.0 port's `DataLayout` self-assignment removal
keeps it clean). `git diff 63b02e2..HEAD -- test/` is **empty** — no golden,
criterion, or harness file changed; the 15.0.0 harness (with
`-Xclang -no-opaque-pointers`, `-no-pie`, and
`-Wno-error=implicit-function-declaration`) works on 16.0.0 as-is. Full
history and root causes: `porting/TestAudit/llvm-16.0.0/SUMMARY.md` →
"Suite results across the port" and "Root-cause fixes".

The 16.0.0 image is `giri-llvm-16` (base **ubuntu:18.04**, prebuilt x86_64
LLVM/Clang **16.0.0** GitHub-Releases tarball
`clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz` at `/usr/local/llvm`,
`llvm-config --version` == 16.0.0). Build/test (inside a `giri-llvm-16`
container; see `porting/AgentGuide.md`):

```bash
docker build -t giri-llvm-16 .          # from the repo root
docker run -it --rm giri-llvm-16 bash
source /giri/utils/build.sh             # cmake + make + make -C test
```

What this port changes on top of the 15.0.0 new-PM head (`63b02e2`). The
inherited new-PM machinery (pass conversion, plugin registration, `mergereturn`
parity, the `Tracer`'s hand-built MAM + built-in-analysis registration, the
`DataLayout` self-assignment removal) is unchanged; the 16.0.0 work is:

- **Toolchain.** `Dockerfile` → `ubuntu:18.04` (the 16.0.0 prebuilt is the
  ubuntu-18.04 GitHub-Releases asset; it links `libtinfo.so.5`, which 20.04 no
  longer ships — `ldd` on 20.04 fails, on 18.04 it is clean. This reverses the
  15.0.0 move to 20.04, which was forced by the rhel-8.4 prebuilt's glibc 2.28
  floor; the ubuntu-18.04 prebuilt needs only glibc ≤ 2.27). `DEBIAN_FRONTEND=noninteractive`
  and the CMake 3.31.12 pin are retained; `utils/install_llvm.sh` gains a
  `16.0.0` case (ubuntu-18.04 tarball, 966,785,280 bytes). **No link shim is
  needed**: the prebuilt's max GLIBCXX symbol (3.4.21) ≤ 18.04's libstdc++
  (3.4.25). `CMakeLists.txt` `find_package(LLVM 15.0 → 16.0 REQUIRED CONFIG)`;
  the 16.0.0 prebuilt's `LLVMConfigVersion.cmake` sets no CMake floor, so the
  project's 3.5 floor remains the binding constraint (16.0.0's soft requirement
  is 3.20; it becomes hard in 17.0.0).
- **C++ standard** (`CMakeLists.txt`). `set(CMAKE_CXX_STANDARD 17)` +
  `CMAKE_CXX_STANDARD_REQUIRED ON`. 16.0.0 is the first release built with
  C++17 by default (`llvm-config --cxxflags` shows `-std=c++17`) and its
  headers use C++17 (`std::is_integral_v` in `llvm/ADT/bit.h`); the prebuilt's
  CMake config exports no `LLVM_OPTIMIZED_CXX_FLAGS` and no standard, so
  without this the project compiled with the host gcc 7.5 `gnu++14` default
  and failed. (The 15.0.0 headers happened to be C++14-compatible, so this was
  latent in the 15 port.)
- **`None` removed** (`lib/Giri/TracingNoGiri.cpp`). The 3.4-vintage
  `getOrInsertF` default arg `ArrayRef<Type *> Args = None` failed to
  compile: 15.0.0's `llvm/ADT/ArrayRef.h` included `llvm/ADT/None.h` and
  carried the implicit `ArrayRef(NoneType) {}` constructor; 16.0.0 dropped the
  transitive include and the constructor (`None.h`/`NoneType` still exist,
  but `None` no longer converts to `ArrayRef`), so `None` was undeclared in
  the translation unit. The default is now `= {}` (empty `ArrayRef`); the sole
  default-using call site (`giriCtor`, a zero-arg function) makes the change
  behavior-identical.
- **`tracer` GLIBCXX shim gate** (`tools/Tracer/CMakeLists.txt`). The 15.0.0
  port hard-wired `llvm_std_shim.cpp` (it back-filled
  `std::__throw_bad_array_new_length` for the 15.0.0 **rhel-8.4** prebuilt,
  whose static libs referenced a symbol the 20.04 host libstdc++ 3.4.28
  lacked). For 16.0.0 the **ubuntu-18.04** prebuilt's static libs reference the
  symbol **0×** (verified `nm -C` over every `llvm-config --libfiles all`
  lib), and 18.04's libstdc++ lacks the `_GLIBCXX_NODISCARD` macro the shim TU
  uses, so compiling it would fail. The CMake now **probes the static libs at
  configure time** and compiles the shim TU only when the symbol is actually
  referenced — correct on 15.0.0 (referenced → compiled) and 16.0.0 (not
  referenced → skipped) alike.
- **Opaque pointers — the expected substantive delta is *not* one for this
  prebuilt.** The 15.0.0 harness's `-Xclang -no-opaque-pointers` is the
  *passthrough* form (forwarded to cc1) and the 16.0.0 ubuntu-18.04 clang
  **honors it**: with the flag the IR is *typed* (`i32*`, `i32**`) — matching
  the 3.4 goldens and why the suite passes 22/22; without it clang 16 emits
  *opaque* `ptr` and the kmeans golden would drift (the very drift the 15.0.0
  port measured). The flag is therefore **load-bearing** and stays in
  `test/Makefile.common` unchanged. (The planning spike's "flag is inert" note
  was wrong on this exact prebuilt — see the task-note correction.)

The three critical invariants are verified (this port):
1. **Numbering determinism** — identical pass sequence in both pipeline stages
   (`-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum"`,
   `test/Makefile.common`), so the same `bbnum`/`lsnum` IDs are assigned in both
   runs; behaviorally proven by the 22/22 PASS against the 3.4 goldens.
2. **`Entry` struct ABI** — `include/Giri/Runtime.h` untouched by this port
   (`git diff 63b02e2..HEAD -- include/Giri/Runtime.h` is empty); re-checked
   in-container: `sizeof(Entry)` = 32 on x86_64 LP64, `4096 % 32 == 0`; every
   fresh `.trace` file `% 32 == 0`; `prtrace` decodes all 22 traces.
3. **Debug info** — `-g` mandatory in the test compile; `clang -g` on 16.0.0
   emits `.debug_info`/`.debug_line`; `SourceLineMapping` yields the 3.4-era
   `file:line` slices across all 22 passing tests.

**The suite measures the seq variants.** `Dockerfile` sets
`TEST_PARALLELISM=seq`, so a suite result says nothing about a pthread variant
unless run by hand. The pthread variants were **not** re-measured for 16.0.0
(the automated suite is the parity gate for this branch); their 8.0.0 findings
stand as the last measurements (`porting/TestAudit/llvm-8.0.0/`).

> The **15.0.0 new-PM** port lives on the sibling branch `port/llvm-15.0.0`
> (working branch `agent/jcode/llvm-15.0.0-port`, head `63b02e2`, PR #21) and
> also reports 22/22 there; this branch cuts from that head and carries its
> `AGENTS.md` baseline (rewritten below). The **14.0.0 new-PM** port
> (`port/llvm-14.0.0`, head `72258e4`, PR #20) and the **14.0.0
> legacy-pass-manager** port (`port/llvm-14.0.0-legacypm`, PR #19, which keeps
> `-enable-new-pm=0` because the legacy PM still exists in 14.0.0) are the
> earlier forward-compat/legacy variants.

## Known residuals

The port is functionally closed (22/22 on the honest seq suite, plus a 22/22
standalone-`tracer` validation). Inherited gaps (never covered by any LLVM
version) are marked [inherited]; regressions (broken by the port) are
[regression]. There are no [regression] rows.

| What | Status | Why acceptable / Evidence |
|------|--------|---------------------------|
| `opt` needs `-load` + `-load-pass-plugin` for each plugin | New-PM-specific, inherent | 16.0.0's new-PM driver discovers passes only via `-load-pass-plugin`; the plugin's `cl::opt` globals still need the plain `-load` (pre-parse dlopen). The harness loads each library twice and documents this. Inherent to 14.0.0+ plugin loading, not a bug. |
| `opt -stats` prints nothing (vs 14.0.0's `Statistics are disabled…`) | [inherited] harmless toolchain behavior | The harness passes `-stats`, but the 16.0.0 prebuilt `opt` prints nothing to stderr for it (0× across the 16.0.0 suite logs), as in 15.0.0. Stderr only — never pollutes the `.ll`/trace/IR files and does not change the slice. |
| Pthread variants not re-measured on 16.0.0 | [inherited] gap (suite scope) | `Dockerfile` pins `TEST_PARALLELISM=seq`; last pthread measurements are the 8.0.0 audit's. Out of the automated suite, same as 5.0.2/8.0.0/14.0.0/15.0.0 |
| test6 (sigusr1), test7 (sigint), test22 (fp) | [inherited] gap | Have golden files but not in `auto-tests.txt`; signals tests need interactive terminal setup, test22 needs `-lm`. Same as 5.0.2/8.0.0/14.0.0/15.0.0 |
| HelloWorld, histogram, linear_regression, word_count | [inherited] gap | No golden file on any LLVM version; not wired into the suite (HelloWorld's Makefile runs by hand — verified on 16.0.0: slice lines `8 10`) |
| `tracer` GLIBCXX shim gated off on 16.0.0 | N/A on this prebuilt / [inherited] mechanism | The 16.0.0 ubuntu-18.04 prebuilt's static libs reference `std::__throw_bad_array_new_length` 0× and 18.04's libstdc++ lacks `_GLIBCXX_NODISCARD`; the configure-time probe skips the shim TU (it compiles on the 15.0.0 rhel-8.4 prebuilt, where two static libs reference the symbol). |
| `ensurePostDomFrontierComputed` — bounded per-function allocation | Replaced / [inherited] benign | `DynamicGiri` builds the `PostDominatorTree` inline (public `Function&` ctor) then runs `PostDominanceFrontier::computeFrontiers`; the per-function tree/frontier is still not freed — same `opt`-process-lifetime bound as the 14.0.0/15.0.0 ports, no observable cost. |
| `signal(SIGKILL, …)` — no-op | [inherited] harmless | `runtime/Giri/Tracing.cpp`. SIGKILL cannot be caught per POSIX. Inherited from 5.0.2/8.0.0/14.0.0/15.0.0 |

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
