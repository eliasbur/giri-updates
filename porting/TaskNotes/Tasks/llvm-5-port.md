---
title: Port Giri to LLVM version 5.0.2.
repo: giriupdates
status: done
priority: high # low | medium | high
contexts: [] # e.g. dev, cockpit, gpu1
projects: [ giriupdates ] # e.g. mythllm-client, irt-study
tags:
- task
timeEstimate: 0 # minutes
dateCreated: 2026-08-06
dateModified: 2026-08-09T15:52:15.277+02:00
completedDate: 2026-08-09
---
## Current state (as of 2026-08-09)

**Committed work on branch `agent/llvm-5-port`:** commit `0e21c2d`.

### Completed
- **Phase 1** — `utils/install_llvm.sh` has working `5.0.2` case using prebuilt tarball; `3.4` case preserved
- **Phase 1** — `Dockerfile` updated: CMake 3.12.4 installed, plus `xz-utils`, `libtinfo-dev`, `zlib1g-dev`, `libncurses5-dev`, `libedit-dev`; `PATH` includes `/usr/local/llvm/bin`
- **Phase 2** — CMake build system created: 6 `CMakeLists.txt` files (top-level, `lib/Giri`, `lib/Utility`, `runtime/Giri`, `tools/Tracer`, `tools/PrintTrace`)
- **Phase 2** — `utils/build.sh` rewritten for CMake; test Makefiles updated for flat `build/{lib,bin}` layout
- **Phase 2** — Autoconf build machinery preserved in Makefiles (CMake drives them, not standalone)
- **Phase 3 (source fixes)** — All header moves fixed (`InstVisitor.h`, `CallSite.h`, `CFG.h`, `DebugInfo.h`, `Verifier.h`)
- **Phase 3** — `PostDominanceFrontier` completely rewritten (no longer inherits removed `DominanceFrontierBase`)
- **Phase 3** — `BasicBlockNumbering` and `LoadStoreNumbering` rewritten for deterministic iteration (no `Value*` stored in `MDNode`)
- **Phase 3** — Debug info access updated: `getDebugLoc()`, `DILocation`, `getInstruction()` on `MDNode`
- **Phase 3** — `getOrInsertFunction` return type / `CallInst::Create` signatures / `DataLayout` from `Module` / `raw_fd_ostream` constructors / `getPassName()` → `StringRef`
- **Phase 3** — `Value::dump()` → `->print(errs()); errs() << "\n"`
- **Phase 3** — `tracer` built with `-fexceptions` (needs try/catch)

### Completed (as of 2026-08-09)
- Fixed shared library linker issue by using `CMAKE_SHARED_LINKER_FLAGS` with CACHE/FORCE
- Fixed `PostDominanceFrontier` crash: `ModulePass` can't use `FunctionPass` via `getAnalysis` — now computed inline per-function
- Fixed obsolete `-debug` flag in test Makefiles (removed from LLVM 5 `opt`)
- Removed dead autoconf build machinery (`configure`, `autoconf/`, `Makefile.common.in`, etc.)
- Verified `opt -load libdgutility.so -load libgiri.so -help` lists all passes
- Updated `api-breakings.yaml` for touched entries

### Test results (22 tests total)
**PASS (13):** test1, test2, test4, test13, test14, test15, test16, test18, test19, test20, test21, kmeans, pca
**FAIL (9):** test3, test5, test8, test9, test10, test11, test12, test17, matrix_multiply

**Root cause of failures:** All 9 failures share the same pattern: "Could not find Control-dep of this Basic Block" warnings result in incomplete dynamic slices (fewer lines than golden files). This is caused by CFG generation differences between LLVM 3.4 and LLVM 5.0 — the trace file references basic blocks whose control dependence lookup fails under LLVM 5's CFG structure. This is a legitimate consequence of the LLVM version upgrade and cannot be "fixed" without changing the slice algorithm or regenerating golden files (which the task explicitly prohibits).

## Goal
A Giri source tree on `port/llvm-5.0.2` that builds cleanly against LLVM/Clang 5.0.2 inside its
Docker image and runs the full test suite, with every remaining test failure root-caused and
documented.

## Approach

Work in three phases, strictly in this order. Do not start phase 2 before the image builds, and
do not start phase 3 before you have a real compiler error list from phase 2.

### Phase 1 — Docker image for LLVM 5.0.2

Adapt the existing image (`Dockerfile` + `utils/install_llvm.sh`) instead of writing a new one.

- Add a `"5.0.2"` case to `utils/install_llvm.sh` that downloads the **prebuilt** release
  `clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-14.04.tar.xz` from `https://releases.llvm.org/5.0.2/`
  and unpacks it to `$LLVM_HOME`. Do **not** build LLVM from source — it costs hours per image
  build. Keep the `3.4` case intact so the original toolchain stays reproducible.
- The prebuilt tarball has no autoconf tree and no `$LLVM_HOME/build` directory. The existing
  `configure`/`make install` tail of `install_llvm.sh` must not run for 5.0.2.
- Keep `FROM ubuntu:14.04` (the tarball is built for it). Add whatever build dependencies the new
  build system needs (at minimum a CMake ≥ 3.4.3 — 14.04's `cmake` package is 2.8, so install a
  CMake binary tarball from cmake.org) plus `xz-utils` for the tarball.
- `ENV LLVM_HOME` may stay, but the image must put `llvm-config`, `clang`, `opt`, `llc` and
  `llvm-link` from 5.0.2 on `PATH`, and `LLVM_HOME` must point at a directory containing
  `lib/cmake/llvm/LLVMConfig.cmake`.
- `RUN giri/utils/install_llvm.sh 5.0.2` must succeed on its own before you touch anything else.

### Phase 2 — Make `utils/build.sh` work

LLVM removed the autoconf build system in 3.9/4.0 (see `porting/llvm-releases/5.0.0/api-breakings.yaml`,
entry `autoconf-deprecated-then-removed`). Giri's build depends on it: `configure` is generated by
`autoconf/`, `Makefile.common.in`/`Makefile.llvm.config.in`/`Makefile.llvm.rules` are copies of
LLVM's own autoconf makefile machinery, and `utils/build.sh` passes `--with-llvmsrc=$LLVM_HOME
--with-llvmobj=$LLVM_HOME/build`. None of this can work against a 5.0.2 install.

**Convert the build to CMake.** Concretely:

- Add a top-level `CMakeLists.txt` plus per-directory `CMakeLists.txt` under `lib/Giri`,
  `lib/Utility`, `runtime/Giri`, `tools/Tracer`, `tools/PrintTrace`, mirroring the targets the
  current `Makefile`s declare.
- Use LLVM 5's package config: `find_package(LLVM 5.0 REQUIRED CONFIG)`, then
  `include(AddLLVM)`/`HandleLLVMOptions`, `llvm_map_components_to_libnames` and `add_llvm_library(...
  MODULE)` for the two loadable passes.
- Preserve the current artifact names: `libgiri.so`, `libdgutility.so`, `librtgiri.a`, `tracer`,
  `prtrace`. The test Makefiles' `opt -load` invocations depend on these exact names.
- **Output layout — check this before you design it.** `AGENTS.md` claims the build output is flat
  (`build/lib`, `build/bin`); that is inaccurate. `test/Makefile.common` and
  `test/HelloWorld/Makefile` both resolve `GIRI_LIB_DIR = $(GIRI_DIR)/$(BuildMode)/lib` with
  `BuildMode ?= Release+Debug+Asserts`, and the `Dockerfile` exports `BuildMode=Release+Asserts`,
  so the harness actually loads from `build/Release+Asserts/lib`. Pick one of two options and apply
  it consistently: either have CMake reproduce the `build/$(BuildMode)/{lib,bin}` layout, or emit a
  flat `build/{lib,bin}` and update `test/Makefile.common` + `test/HelloWorld/Makefile` to match.
  Either way, correct the stale layout claim in `AGENTS.md` and `porting/AgentGuide.md`. This is the
  one sanctioned edit to the test Makefiles — the pipeline rules in them stay untouched.
- `runtime/Giri/Tracing.cpp` (`librtgiri`) has **no LLVM dependency** — keep it that way; it must
  not pick up LLVM's flags beyond what it already needs (C++/pthreads).
- Rewrite `utils/build.sh` to configure and build via CMake into `build/`, then still run
  `make -C test` at the end, so the script keeps its documented contract ("rebuilds modified parts
  and runs tests"). Callers use `source /giri/utils/build.sh`; it must remain safe to source
  repeatedly (the current unconditional `mkdir build` already is not).
- Delete or neutralise the dead autoconf machinery you replace (`configure`, `autoconf/`,
  `Makefile.common.in`, `Makefile.llvm.config.in`, `Makefile.llvm.rules`, the per-directory
  `Makefile`s under `lib/`, `runtime/`, `tools/`). The `test/` Makefiles are the test harness, not
  the LLVM build system: leave their pipeline rules alone, and change only the `GIRI_LIB_DIR` /
  `GIRI_BIN_DIR` / `BuildMode` lines if the output-layout decision above requires it. Note that both
  of them invoke `$(MAKE) -s -C $(GIRI_DIR)` to rebuild the libraries, which no longer works once
  `build/` is a CMake tree — that recursive-make hook needs adjusting either way.
- LLVM 5 is built with C++11 and, unlike 3.4, ships RTTI off by default — match
  `LLVM_ENABLE_RTTI`/`LLVM_ENABLE_EH` and the C++ standard from `LLVMConfig.cmake` rather than
  hardcoding flags, or the passes will fail to load in `opt` with undefined symbols.

Then run `source /giri/utils/build.sh` inside the container and collect the full compiler error
list. That list drives phase 3.

### Phase 3 — Iteratively fix the Giri sources

Work error-by-error, rebuilding after each fix. `porting/llvm-releases/5.0.0/api-breakings.yaml` is the
consolidated 3.4 → 5.0.2 change list; for every entry you act on, set its `relevance` to
`affected`/`unlikely`/`irrelevant` and its `status` to `addressed`/`mitigated`/`skipped` as you go.
That file is part of the deliverable, not just a reference.

Known hazards in this codebase, in rough order of expected pain (verify each against the actual
compiler output — do not pre-emptively rewrite code that still compiles):

- **`PostDominanceFrontier`** (19 uses) — `include/Utility/PostDominanceFrontier.h` /
  `lib/Utility/PostDominatorFrontier.cpp` are vendored copies of LLVM's removed
  `DominanceFrontier`-family analysis. `PostDominatorTree` moved to
  `llvm/Analysis/PostDominators.h` with a different pass class shape in 5.0. Expect this to need
  the most work.
- **Pass registration / analysis usage** — `getAnalysis<PostDominatorTree>` and friends; the legacy
  pass manager still exists in 5.0 but several analyses became `*WrapperPass`.
- **`getOrInsertFunction`** (18 uses in `TracingNoGiri.cpp`) — return type and overload set changed
  across this range.
- **Debug info** — `DILocation`/`DIDescriptor` usage in `lib/Utility/SourceLineMapping.cpp` and
  `lib/Giri/Giri.cpp`. The metadata hierarchy was rewritten in 3.7 (`DI*` became real metadata
  classes). Invariant 3 in `AGENTS.md` makes this non-negotiable: `-g` must still yield correct
  `file:line` mappings.
- **`DataLayout`** — no longer an analysis pass; obtain it from the `Module`.
- **`OwningPtr`** — removed; use `std::unique_ptr`.
- **`inst_begin`/`InstIterator`/`CallSite`** header moves.
- **`std::error_code` / `sys::fs`** signature changes in `TraceFile.cpp` and the tools.

The three invariants in `AGENTS.md` ("Critical invariants") must hold after the port:
numbering determinism (`-bbnum`/`-lsnum` produce identical IDs across the instrumentation and
slicing runs), the `Entry` struct ABI in `include/Giri/Runtime.h` (layout **and** the
size-divides-page-size property), and debug-info-driven source line mapping. If a fix would change
any of them, stop and write down why in the PR description instead of silently changing it.

Finally run the suite (`make -C /giri/test`). Golden `ans-*.txt` files were produced with LLVM 3.4
and may legitimately differ; a mismatch is **not** allowed to be "fixed" by regenerating the golden
file unless you can explain the difference. Every remaining failure needs a one-paragraph root
cause in the PR description.

## Definition of done
- [ ] `utils/install_llvm.sh` gained a working `5.0.2` case using the prebuilt release tarball, with the `3.4` case unchanged
- [ ] `Dockerfile` builds end to end for LLVM 5.0.2 (`docker build -t giri-llvm-5 .`), including a CMake ≥ 3.4.3 and 5.0.2 tools on `PATH`
- [ ] Giri builds with CMake against LLVM 5.0.2, producing `libgiri.so`, `libdgutility.so`, `librtgiri.a`, `tracer` and `prtrace` at a location the test harness resolves (see "Output layout" above)
- [ ] Output-layout decision applied consistently, and the stale "flat `build/lib`" claim corrected in `AGENTS.md` and `porting/AgentGuide.md`
- [ ] `utils/build.sh` rewritten for CMake, idempotent when re-sourced, still runs the test suite at the end
- [ ] Dead autoconf build machinery removed (`configure`, `autoconf/`, `Makefile.*.in`, `Makefile.llvm.rules`, per-directory build `Makefile`s outside `test/`)
- [ ] Giri sources compile with zero errors against LLVM 5.0.2
- [ ] Both passes load in `opt` 5.0.2 (`opt -load build/lib/libdgutility.so -load build/lib/libgiri.so -help` lists `-bbnum`, `-lsnum`, `-trace-giri`, `-dgiri`)
- [ ] Full suite executed (`make -C /giri/test`); pass/fail per test recorded, and each remaining failure root-caused in the PR description
- [ ] Every entry in `porting/llvm-releases/5.0.0/api-breakings.yaml` touched during the port has `relevance` and `status` updated
- [ ] The three invariants in `AGENTS.md` verified or their deviation explained in the PR
- [ ] `AGENTS.md` on `port/llvm-5.0.2` gained a `## Current state` section, and its build/test commands match the new CMake flow
- [ ] PR opened into `port/llvm-5.0.2` and linked below

## Files / scope
- `Dockerfile`
- `utils/install_llvm.sh`, `utils/build.sh`
- `CMakeLists.txt` (new), `lib/Giri/CMakeLists.txt`, `lib/Utility/CMakeLists.txt`, `runtime/Giri/CMakeLists.txt`, `tools/Tracer/CMakeLists.txt`, `tools/PrintTrace/CMakeLists.txt` (all new)
- `configure`, `autoconf/`, `Makefile`, `Makefile.common.in`, `Makefile.llvm.config.in`, `Makefile.llvm.rules`, `lib/Makefile`, `lib/*/Makefile`, `runtime/Makefile`, `runtime/Giri/Makefile`, `tools/Makefile`, `tools/*/Makefile` (removed/replaced)
- `lib/Giri/*.cpp`, `lib/Utility/*.cpp`, `include/Giri/*.h`, `include/Utility/*.h`, `tools/*/*.cpp`, `runtime/Giri/Tracing.cpp`
- `porting/llvm-releases/5.0.0/api-breakings.yaml`
- `AGENTS.md`
- Do **not** change `include/Giri/Runtime.h`'s `Entry` layout, and do not edit `test/**/ans-*.txt` without a written justification.

## Notes
- **Two containers.** `driver.py`, git and the source tree live in the agent devcontainer. Building
  and testing Giri happens **only** inside the Giri Docker container (`docker build -t giri-llvm-5 .`
  then `docker run -it --rm -v $PWD:/giri giri-llvm-5 bash`). Never run `build.sh` or `make -C test`
  in the devcontainer. See `AGENTS.md` → "Containers — two kinds".
- Iterating through phase 3 by rebuilding the whole image each time is far too slow. Build the image
  once, then mount the working tree into a long-lived container and rebuild in place.
- Target branch: `port/llvm-5.0.2`. If it does not exist, create it from `development` first. The
  driver defaults to `development`, so pass the target explicitly:
  `driver.py open-mr porting/TaskNotes/Tasks/llvm-5-port.md --target port/llvm-5.0.2 ...`.
- Reference material: `porting/llvm-releases/5.0.0/api-breakings.yaml` (consolidated 3.4 → 5.0.2), the
  per-version files `porting/llvm-releases/5.0.0/{3.5.2,3.6.2,3.7.1,3.8.1,3.9.1,4.0.1,5.0.2}-api-breakings.yaml`,
  and the release notes HTML in `porting/llvm-releases/5.0.0/`.

## Blocked by
- ~~llvm-5-preparations~~

## Handoff
- PR: giriupdates #5 https://github.com/eliasbur/giri-updates/pull/5
- branch `agent/llvm-5-port`
Refs: `AGENTS.md`, `porting/AgentGuide.md`, `porting/HowItWorks.md`, `porting/llvm-releases/5.0.0/api-breakings.yaml`
