---
title: Port Giri to LLVM 15.0.0 (new pass manager; the legacy PM is gone in 15).
status: open
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-27
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal

A Giri source tree on `port/llvm-15.0.0` that builds cleanly against LLVM/Clang
15.0.0 (prebuilt-tarball toolchain), whose execution path is the **new pass
manager** — 15.0.0 is the first release where the **legacy pass manager is
gone** ("deprecated in 14, removed after 14"), so only the new-PM code path
survives it — runs the full test suite on the honest harness with new-PM
`opt -passes` pipelines, with every remaining test failure root-caused and
documented, and the three critical invariants preserved.

This is the continuation of the new-PM line. The base is the completed
14.0.0 new-PM head (`agent/jcode/llvm-14-newpm-port` @ `72258e4`, PR #20),
where **all** passes are already new-PM classes, both plugins export
`llvmGetPassPluginInfo`, the harness uses `-load` + `-load-pass-plugin` +
`-passes`, and the `Tracer` builds its `ModuleAnalysisManager` by hand. The
8→14 and 14→new-PM work is therefore already done; this port applies only the
**14.0.0 → 15.0.0** delta (two minor versions) on top.

## Approach

The 14.0.0 new-PM port (`agent/jcode/llvm-14-newpm-port`, 22/22) is the base:
pass conversion, plugin registration, harness, and the `Tracer` hand-built-MAM
fix are already done and inherited. What this port changes on top:

1. **Toolchain (the main work of this port).**
   - `utils/install_llvm.sh`: new `"15.0.0"` case. 15.0.0 prebuilts live on the
     GitHub Releases of llvm/llvm-project (tag `llvmorg-15.0.0`), but there is
     **no ubuntu-18.04 asset** — the only x86_64-linux prebuilt is
     `clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz` (verified against the
     GitHub API). The rhel-8.4 tarball is glibc 2.28, so the base image must
     provide glibc ≥ 2.28 → **bump the Dockerfile base to `ubuntu:20.04`**
     (glibc 2.31; gcc 9.4 covers the C++17 that 15.0.0 builds with, the first
     LLVM to require C++17).
   - `Dockerfile`: `FROM ubuntu:20.04`; bump the pinned CMake binary as far as
     possible (15.0.0 requires CMake ≥ 3.13.4; the 14.0.0 image pinned 3.12.4,
     below that). Pin a known-good recent CMake Linux-x86_64 binary and record
     the version here.
   - `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.4.3)` → `3.13.4`
     (15.0.0's requirement) so a recent CMake does not emit a < 3.10
     compatibility deprecation.
2. **14→15 API deltas (expected small; verify, don't assume).** The 14.0.0
   new-PM code uses no legacy-PM API (zero residual, verified). Known 14→15
   breaks that could touch Giri:
   - C++17 mandatory — the Giri C++ is already clean of the removed constructs
     (no `register`, `auto_ptr`, `unary_function`, `random_shuffle` — grepped).
   - `PassPluginLibraryInfo` / `opt -load` / `-load-pass-plugin` behavior —
     re-spike in 15.0.0 (14.0.0 had 4 fields; confirm 15.0.0 still resolves
     `llvmGetPassPluginInfo` and registers the passes, and that the plain
     `-load` still dlopens the plugins' `cl::opt` globals pre-parse).
   - `PostDominatorTree(Function&)` public ctor + `properlyDominates` — the
     14.0.0 `DynamicGiri` path builds it inline; confirm the ctor signature is
     unchanged in 15.0.0 (it changed in 16.0.0, not here).
   - `mergereturn` new-PM availability + byte-identical legacy-vs-new-PM —
     re-confirm on 15.0.0 (inherited invariant-1 spike).
   - `DataLayout` calls in `lib/Giri/TracingNoGiri.cpp` /
     `tools/Tracer/Tracer.cpp` — stable across 14→15; confirm.
   Fix whatever the 15.0.0 compile + spike surface, keeping pass logic
   byte-identical.
3. **Tests + audit + invariants.** Honest seq suite (`make -C test`,
   `TEST_PARALLELISM=seq`) in the `giri-llvm-15` container against the pristine
   3.4 goldens; per-test root-cause audit at `porting/TestAudit/llvm-15.0.0/`;
   re-verify the three invariants (ABI vs `72258e4` + `sizeof(Entry)=32`,
   `-g` `file:line` slices, numbering determinism + `mergereturn` parity).
4. **Standalone-tool whole-result validation.** Re-run
   `porting/TestAudit/llvm-14.0.0-newpm/_tool_validation/full_tool_validation.py`
   (22/22 through the `tracer` CLI + `prtrace` on all 22 traces) and the
   `test/HelloWorld` harness; fix any tool regression root-cause style (the
   14.0.0 `Tracer` MAM-registration fix is inherited, but 15.0.0's
   `ModulePassManager::run` built-in-analysis set must be re-confirmed).

**Spike (before the full build):** in a scratch `giri-llvm-15` container:
(a) the 15.0.0 rhel-8.4 prebuilt loads on `ubuntu:20.04` (`ldd` clean, no
`GLIBC_x` not-found), `llvm-config --version` == 15.0.0, record
`--assertion-mode` (the 14.0.0 prebuilt was Release/assertions-OFF);
(b) a minimal new-PM probe plugin registers a pass via `llvmGetPassPluginInfo`
and runs under `opt -load-pass-plugin` (confirm the 4-field info struct +
cross-library resolution still hold); (c) `mergereturn` new-PM is byte-identical
to the 14.0.0 transform on a multi-return function; (d) `PostDominatorTree(Function&)`
builds. Record findings in the progress log; a negative spike on (a)–(b)
changes the toolchain approach (base-image bump / source build) and must be
surfaced before phase 2.

## Definition of done

- [ ] Spike: 15.0.0 prebuilt loads on `ubuntu:20.04` (`ldd` clean);
      `llvm-config --version` == 15.0.0; assertion-mode recorded; new-PM probe
      plugin + `mergereturn` + `PostDominatorTree` verified
- [ ] Toolchain wired: `install_llvm.sh` 15.0.0 case, `Dockerfile`
      (`ubuntu:20.04` + CMake bump + `install_llvm.sh 15.0.0`),
      `CMakeLists.txt` `cmake_minimum_required` bump
- [ ] Build green in `giri-llvm-15`: 5 artifacts in `build/{lib,bin}`;
      `opt -passes=...` lists/runs the Giri passes; any 14→15 API fixes keep
      pass logic byte-identical
- [ ] Honest-harness seq suite run in `giri-llvm-15`; per-test results + root
      causes at `porting/TestAudit/llvm-15.0.0/` (target 22/22 PASS, rc=0)
- [ ] The three critical invariants re-verified (ABI / `-g` / numbering)
- [ ] `git diff 72258e4..HEAD -- test/` empty (or only documented harness deltas)
- [ ] Standalone-tool whole-result validation: 22/22 via `tracer` CLI +
      `prtrace` on all traces + `test/HelloWorld` harness
- [ ] Change data: 14→15 breaks (C++17, legacy-PM removal, PassPlugin/analysis
      deltas) recorded at `porting/llvm-releases/15.0.0/`
- [ ] `AGENTS.md` branch copy updated (Current state + Known residuals)
- [ ] PR opened into `port/llvm-15.0.0` and linked below

## Files / scope

- `Dockerfile` (ubuntu:20.04 base, CMake bump, `install_llvm.sh 15.0.0`)
- `utils/install_llvm.sh` (15.0.0 case)
- `CMakeLists.txt` (`cmake_minimum_required` bump, if the recent CMake warns)
- `lib/`, `tools/`, `include/` — **only if** the spike/build surface a 14→15
  API break in Giri code (expected minimal: the 14.0.0 new-PM code uses no
  legacy-PM API and the C++ is already C++17-clean)
- `porting/llvm-releases/15.0.0/` (change data: `15.0.0-api-breakings.yaml`,
  Release Notes, `api-breakings.yaml` aggregator)
- `porting/TestAudit/llvm-15.0.0/`, `AGENTS.md` (evidence)

Out of scope: `include/Giri/Runtime.h` (ABI), test cases/goldens/criterion
files, and the pass pipeline in `test/Makefile.common` (invariant 1 — the
14.0.0 new-PM harness is expected to carry over unchanged; a delta there would
need to be justified and documented).

## Blocked by

- ~~llvm-14-newpm-port (done, PR #20; the base is the new-PM head `72258e4`)~~

## Progress log

## Handoff

- branch `agent/jcode/llvm-15.0.0-port`
- PR: <auto-written by driver.py finish>
Refs: `porting/AgentGuide.md`, `porting/HowItWorks.md`, `llvm-14-newpm-port.md`,
`porting/TestAudit/llvm-14.0.0-newpm/SUMMARY.md`
