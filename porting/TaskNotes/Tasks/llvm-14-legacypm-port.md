---
title: Port Giri to LLVM 14.0.0 (legacy pass manager variant).
status: done
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []             # e.g. dev, cockpit, gpu1
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-25
# dateModified / completedDate are added automatically by `driver.py finish`
dateModified: 2026-08-26T08:38:39.429+02:00
completedDate: 2026-08-26
---
## Goal

A Giri source tree on `port/llvm-14.0.0-legacypm` that builds cleanly against LLVM/Clang
14.0.0 (prebuilt-tarball toolchain) inside its Docker image, keeps the legacy pass
manager as the execution path (opt 14 run with `-enable-new-pm=0`), runs the full test
suite on the honest harness, with every remaining test failure root-caused and
documented, and the three critical invariants preserved.

This is the "easy" 14.0.0 port. A forward-compatible new-pass-manager variant is
tracked separately in `llvm-14-newpm-port.md` on branch `port/llvm-14.0.0`, cut from the
completed branch of this task.

## Approach

Five phases, in order. The legacy pass manager is still fully functional in LLVM 14
(new PM is the *default* for the opt pipeline but legacy is explicitly supported via
`-enable-new-pm=0` per the 14.0.0 docs), so no pass rewrites are needed — only the
8→14 API migrations.

### Phase 0 — toolchain (gate everything)

- `utils/install_llvm.sh`: add a `14.0.0` case. LLVM stopped shipping prebuilt
  tarballs on releases.llvm.org after 9.0.0; from 10.0.0 onward they live on GitHub
  Releases (`llvm/llvm-project`, tag `llvmorg-14.0.0`). The 14.0.0 x86_64 Linux
  prebuilts are `clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-{18.04,...}.tar.xz`; use the
  ubuntu-18.04 one (0.60 GB, verified live 2026-08-25). Base image stays
  `ubuntu:18.04` (the tarball is built for it; gcc 7.5 covers C++14).
- `Dockerfile`: new image `giri-llvm-14` from `ubuntu:18.04`, same apt set as the 8.0.0
  image, `RUN giri/utils/install_llvm.sh 14.0.0`, with a comment recording the
  GitHub-Releases provenance and that a source build is the fallback only if the
  tarball route breaks.
- Root `CMakeLists.txt`: `find_package(LLVM 14.0 REQUIRED CONFIG)`; C++ standard to
  cxxflags (14.0.0 builds with C++14 per `llvm-config --cxxflags`).
- Spike inside the image: `llvm-config --version` == 14.0.0; `llvm-config --cxxflags`;
  a trivial `RegisterPass` pass .so loads and runs through `opt -enable-new-pm=0`
  (proves the legacy plugin path still works on 14); `llc --help` for
  `-asm-verbose` (removed in the 10-era; if gone, a one-line harness fix in
  `test/Makefile.common`, pre-approved as a harness change).

### Phase 1 — 8→14 API fixes (PM-agnostic, zero legacy-PM change)

- `CallSite` was removed in LLVM 12: `lib/Giri/TracingNoGiri.cpp`,
  `lib/Giri/TraceFile.cpp`, `lib/Utility/SourceLineMapping.cpp` → `CallBase`.
- `tools/Tracer/Tracer.cpp`: `WriteBitcodeToFile` now returns `Error` (changed in
  LLVM 12) — handle the error. `legacy::PassManager` stays (this branch is the
  legacy-PM variant).
- Carried over from the 8.0.0 port: the four `DEBUG` compat shims.
- Gate: zero compile errors, all 5 artifacts in `build/{lib,bin}`, and
  `opt -enable-new-pm=0 -help` lists `-bbnum -lsnum -trace-giri -dgiri`.

### Phase 2 — harness

- `test/Makefile.common`: prepend `-enable-new-pm=0` to every `opt -load ...`
  invocation (three pipeline invocations + the mapping/bbid/lsid targets).
  Test cases, goldens, and criteria stay untouched.

### Phase 3 — tests, audit, invariants

- Honest seq suite (`make -C test`, `TEST_PARALLELISM=seq`) against the pristine 3.4
  goldens; per-test root-cause audit at `porting/TestAudit/llvm-14.0.0-legacypm/`
  (SUMMARY + per-test reports, 8.0.0 precedent). Manual pthread runs. Any 8→14
  codegen criterion drift is recorded `FAIL-EXPECTED` with the corrected
  matrix_multiply methodology (no file changes without explicit consent).
- Re-verify the three critical invariants (`Entry` ABI byte-identical, `-g`
  mandatory, identical `-bbnum`/`-lsnum` numbering in both pipeline stages).
- Change data at `porting/llvm-releases/14.0.0/` (per-version 9.0.0–14.0.0 +
  consolidated, 8.0.0 precedent; no `dockerfile-snippet.yaml` per the recorded
  8.0.0 decision). `AGENTS.md` branch copy: `## Current state` + `## Known residuals`.

### Phase 4 — handoff

- PR into `port/llvm-14.0.0-legacypm`; `driver.py finish` links the PR and marks the
  note done.

## Definition of done

- [x] `install_llvm.sh 14.0.0` case present (GitHub-Releases tarball); 3.1/3.4/5.0.2/8.0.0 cases byte-untouched
- [x] `giri-llvm-14` image (ubuntu:18.04) builds; `llvm-config --version` == 14.0.0
- [x] CMake pins `find_package(LLVM 14.0 REQUIRED CONFIG)`; pin enforcement proven (negative-version configure control)
- [x] Spike: legacy plugin loads under `opt -enable-new-pm=0`; `llc -asm-verbose` fate checked
- [x] 8→14 API fixes compile clean; 5 artifacts in `build/{lib,bin}`; `opt -enable-new-pm=0 -help` lists the 4 Giri passes
- [x] Honest-harness seq suite run; per-test results + root causes at `porting/TestAudit/llvm-14.0.0-legacypm/`
- [x] The three critical invariants re-verified (ABI / `-g` / numbering)
- [x] `git diff <base>..HEAD -- test/` empty except pre-approved harness lines in `test/Makefile.common`
- [x] Change data `porting/llvm-releases/14.0.0/` authored, triaged, schema- and union-consistent
- [x] `AGENTS.md` branch copy updated (Current state + Known residuals)
- [x] PR opened into `port/llvm-14.0.0-legacypm` and linked below

## Files / scope

- `utils/install_llvm.sh`, `Dockerfile`, `CMakeLists.txt` (toolchain)
- `lib/Giri/TracingNoGiri.cpp`, `lib/Giri/TraceFile.cpp`, `lib/Utility/SourceLineMapping.cpp`, `tools/Tracer/Tracer.cpp` (8→14 API)
- `test/Makefile.common` (harness: `-enable-new-pm=0`)
- `porting/TestAudit/llvm-14.0.0-legacypm/`, `porting/llvm-releases/14.0.0/`, `AGENTS.md` (evidence)

## Blocked by

- ~~port/llvm-8.0.0 completed (agent branch agent/open-code/llvm-8-port @ 224bdfb)~~ — cut from its head; PR #18 into `port/llvm-8.0.0` is open and independent.

## Progress log

- 2026-08-25 — **Full build green + test parity on 14.0.0 (legacy-PM).** All 8→14 API
  fixes compile clean; `giri`, `dgutility`, `rtgiri`, `prtrace`, and `tracer` all build.
  Reverted the bad `STATISTIC_WITH_TYPE` shim (commit `f0f0f9c`): 14.0.0 headers
  `#undef DEBUG_TYPE`, so `DEBUG_TYPE` is now defined *after* the includes and the
  bare `DEBUG(X)` macro is re-`#define`d via `DEBUG_WITH_TYPE` where used.
  - API fixes: `FunctionCallee` (`getOrInsertFunction(...).getCallee()`) in
    `include/Utility/Utils.h` + `lib/Giri/TracingNoGiri.cpp`; `CallBase::getCalledOperand()`
    for removed `getCalledValue()` (`SourceLineMapping.cpp`, `TracingNoGiri.cpp`);
    `sys::fs::F_*`→`OF_*` (`Giri.cpp`, `SourceLineMapping.cpp`, `Tracer.cpp`);
    `TraceFile.cpp` `const CallBase* CS = cast<CallBase>(I)` + `CS->arg_size()`/
    `getArgOperand(i)` for removed `CallSite`; `TracingNoGiri` converted from removed
    `BasicBlockPass` to `FunctionPass` (per-BB behavior preserved via `runOnFunction`);
    `#include <map>` in `LoadStoreNumbering.h`; `Twine` disambiguation in
    `CountSrcLines.cpp`.
  - **`tracer` link:** the prebuilt LLVM 14.0.0 CMake package does not populate
    `LLVM_COMPONENT_LIBS`, so `llvm_map_components_to_libnames` is empty. `tools/Tracer/CMakeLists.txt`
    now queries `llvm-config --libfiles all` (163 dependency-ordered static libs, linked by
    absolute path) plus `--system-libs` (`-lrt -ldl -lpthread -lm`).
  - **Test suite: 22 PASS / 0 FAIL** (UnitTests test1–test5, test8–test21 +
    `matrix_multiply`/`pca`/`kmeans`); every `opt` invocation runs `-enable-new-pm=0`
    (legacy PM). Log: `.llvm14log/make_full10.log`.
  - **Invariants re-verified in-container:** Entry `sizeof`=32, `4096 % 32 == 0`
    (page-divisible); `-g` emits `.debug_info`/`.debug_line`; numbering determinism
    exercised by the passing parity suite.
- 2026-08-26 `8bc76dd` — **Phase 3 closeout: change-data YAMLs + per-test audit +
  AGENTS.md branch copy.** Authored and triaged (in place) the 6 per-version release-note
  extractions under `porting/llvm-releases/14.0.0/` (9.0.0–14.0.0, 352 items; raw HTML
  sources committed alongside, 8.0.0/5.0.0 precedent), and generated the consolidated
  `api-breakings.yaml` on the 8.0.0 model: 350 entries = 346 deduped per-version union
  (cross-version groups merged with `versions[]`/`originalIds` refs: powerpc POWER10
  11+12, mips/avr/webassembly/amdgpu redacted markers 12+14, legacy-PM deprecation
  13+14) + 4 header-level port-critical breaks that are **not citable from any release
  note** (getOrInsertFunction→FunctionCallee 9.0.0, CallSite→CallBase 11.0.0,
  sys::fs F_*→OF_* 13.0.0, DEBUG_TYPE `#undef` via `GenericDomTreeConstruction.h` newly
  pulled in by `Dominators.h` in 14.0.0 — the STATISTIC macro references DEBUG_TYPE at
  the call site; fixed by `DEBUG_TYPE` after the includes, commit `74b870f`). Every
  version attribution pinned against the prebuilt 14.0.0 container headers + raw
  `llvmorg-X` GitHub headers. Also: `porting/TestAudit/llvm-14.0.0-legacypm/`
  (SUMMARY + 22 per-test reports) and the `AGENTS.md` branch copy (`## Current state` +
  `## Known residuals`; corrected the FunctionCallee/F_None version attributions to the
  header-verified 9.0.0/13.0.0). Validation: all 7 YAMLs parse, schema-clean,
  union-consistent (all `originalIds` resolve; no per-version item unrepresented).
  All 11 DoD items checked; PR #19 opened into `port/llvm-14.0.0-legacypm`. next: (none —
  this task is done; the new-PM port continues in `llvm-14-newpm-port.md` on
  `port/llvm-14.0.0`).

## Handoff

- branch `agent/open-code/llvm-14-legacypm`
- PR: giri-updates #19 https://github.com/eliasbur/giri-updates/pull/19
Refs: `porting/AgentGuide.md`, `porting/HowItWorks.md`, `llvm-8-port.md`
