---
title: Port Giri to LLVM 16.0.0 (new pass manager; opaque pointers are now the only IR mode)
status: done
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-31
# dateModified / completedDate are added automatically by `driver.py finish`
dateModified: 2026-09-01T02:29:00.677+02:00
completedDate: 2026-09-01
---

## Goal

A Giri source tree on `port/llvm-16.0.0` that builds cleanly against LLVM/Clang
16.0.0 (prebuilt-tarball toolchain), whose execution path is the **new pass
manager** — 16.0.0 **removed the legacy pass manager** (the deprecation that
started in 14.0.0 took effect; the `--enable-new-pm` option remains as a
migration aid only), and it **built with C++17 by default** (the new hard
requirement is GCC ≥ 7.1 / CMake ≥ 3.20-soft). The port runs the full test
suite on the honest harness with new-PM `opt -passes` pipelines against the
**unmodified 3.4-era goldens**, with every remaining test failure root-caused
and documented, and the three critical invariants preserved.

This is the continuation of the new-PM line. The base is the completed 15.0.0
new-PM head (`agent/jcode/llvm-15.0.0-port` @ `63b02e2`, PR #21 — still open
at cut time), where **all** passes are new-PM classes, both plugins export
`llvmGetPassPluginInfo`, the harness uses `-load` + `-load-pass-plugin` +
`-passes`, and the `Tracer` builds its `ModuleAnalysisManager` by hand. The
8→14, 14→new-PM, and 14→15 work is therefore already done; this port applies
only the **15.0.0 → 16.0.0** delta on top.

## Approach

The 15.0.0 new-PM port (`agent/jcode/llvm-15.0.0-port`, 22/22) is the base:
pass conversion, plugin registration, harness, and the `Tracer` hand-built-MAM
fix are already done and inherited. What this port changes on top:

1. **Toolchain (spike done, 2026-08-31).**
   - `utils/install_llvm.sh`: new `"16.0.0"` case. 16.0.0 prebuilts live on
     the GitHub Releases of llvm/llvm-project (tag `llvmorg-16.0.0`); the
     x86_64-linux GNU asset is
     `clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz`
     (966,785,280 bytes, verified against the GitHub API asset list).
   - **Base image: `ubuntu:18.04`** (down from 20.04 in the 15 port). The
     16.0.0 prebuilt is an ubuntu-18.04 build that links `libtinfo.so.5`;
     bare `ubuntu:20.04` provides only `libtinfo.so.6`, so the 20.04 base
     cannot load the prebuilt `opt`/`clang`. On `ubuntu:18.04` (glibc 2.27,
     gcc 7.5) the spike was clean: `ldd` on `opt`/`clang` = 0 missing; max
     symbol requirement `GLIBCXX_3.4.21` ≤ 18.04's libstdc++ `GLIBCXX_3.4.25`
     → **no link shim needed** (unlike the 15.0.0 rhel-8.4 prebuilt).
   - `llvm-config --version` = 16.0.0; `--cxxflags` shows `-std=c++17`
     (16.0.0 is the first release built with C++17 by default; GCC ≥ 7.1 is
     the new hard toolchain requirement — 18.04's gcc 7.5 satisfies it).
   - CMake: 16.0.0 wants ≥ 3.20 (soft in 16.x, hard in 17.x); the port keeps
     the pinned 3.31.12 binary and the 3.5 project floor (both already
     satisfy it). `find_package(LLVM 16.0 REQUIRED CONFIG)` (was 15.0).
2. **Opaque pointers (the substantive delta of this port — RESOLVED,
     corrected 2026-08-31 15-00, the spike finding was wrong).**
   - ~~16.0.0's clang emits opaque-pointer IR only and the harness's
     `-Xclang -no-opaque-pointers` flag is gone~~ **DISPROVEN on this exact
     prebuilt (16.0.0 ubuntu-18.04 asset):** the flag is the *passthrough* form
     (forwarded to cc1) and clang 16 honors it. Measured: with the flag the IR
     is *typed* (`i32*`, `i32**`); without it clang 16 emits *opaque* `ptr`.
     The flag is therefore **load-bearing** (removing it re-introduces the
     kmeans golden drift the 15.0.0 port measured) and **stays** in
     `test/Makefile.common` unchanged — no harness edit is needed for 16.0.0,
     so `git diff 63b02e2..HEAD -- test/` is empty.
   - **Golden-reachability question (empirical gate).** The 3.4-era goldens
     were recorded under typed-pointer IR; the 15.0.0 port measured an
     opaque-IR drift once (kmeans line set 61/240/279 vs golden 222/276),
     which is why it pinned typed emit. RESOLVED: because the harness flag keeps the IR *typed* (see above),
     the 16.0.0 toolchain (new Clang 16 codegen for `-O0`, new `opt` passes)
     reproduces all 22 line sets — the honest `TEST_PARALLELISM=seq` run
     (2026-08-31 14-41) passed 22/22 with no golden edit. — was originally:
     decided by the honest seq suite run —
     the goldens and criterion files are **not modifiable**. Any failing
     test is root-caused per-test and documented in
     `porting/TestAudit/llvm-16.0.0/`; the expectation is that most tests
     are type-insensitive (the slice is use-def based over instructions),
     with the observed exceptions documented, not hidden.
   - **Library compile fixes (15→16 header breaks).** `lib/Giri/TracingNoGiri.cpp:94`
     and `lib/Giri/TraceFile.cpp:685` use `PointerType::getUnqual(Int8Type)`;
     the 16.0.0 headers still provide `getUnqual` (it now returns an opaque
     `ptr`), so both sites are expected to compile unchanged — the C++17
     mandate adds no new breaks (tree scanned: no `register`, `random_shuffle`,
     `throw()`, `auto_ptr`, `tr1`). Any other 15→16 break is fixed
     root-cause with the pass logic kept byte-identical where possible.
3. **Harness parity (honest suite).**
   - `test/Makefile.common` + `test/HelloWorld/Makefile` only. The `-no-pie`
     link flag (15.0.0 delta) is expected to carry over (clang 16 links PIE
     by default; `llc -O0` emits static-model asm) — verify per build.
   - Keep `TEST_PARALLELISM=seq` pinned (Dockerfile `ENV`), `-g` mandatory,
     and the identical pass sequence in both pipeline stages (numbering
     determinism).
4. **Evidence + change data + docs.**
   - `porting/TestAudit/llvm-16.0.0/` (SUMMARY.md + per-test results + suite /
     standalone transcripts).
   - `porting/llvm-releases/16.0.0/api-breakings.yaml` (release-note
     extraction + triage) and the consolidated file extended to
     `8.0.0-16.0.0`.
   - `AGENTS.md` branch copy (Current state + Known residuals),
     source-grounded like the 15.0.0 one.

## Definition of done

- [ ] Spike: 16.0.0 prebuilt loads on `ubuntu:18.04` (`ldd` clean);
      `llvm-config --version` == 16.0.0; `-std=c++17` recorded; no GLIBCXX
      gap (no shim); opaque-only confirmed; `-load`/`-load-pass-plugin`
      plugin mechanism + `mergereturn` in `opt -passes` verified
- [ ] Toolchain wired: `install_llvm.sh` 16.0.0 case, `Dockerfile`
      (`ubuntu:18.04` + CMake 3.31.12 pinned + `install_llvm.sh 16.0.0` +
      `DEBIAN_FRONTEND=noninteractive`), `CMakeLists.txt` `find_package` 16.0
- [ ] Build green in `giri-llvm-16`: 5 artifacts in `build/{lib,bin}`;
      `opt -passes=...` lists/runs the Giri passes; any 15→16 API fixes keep
      pass logic byte-identical
- [ ] Honest-harness seq suite run in `giri-llvm-16`; per-test results + root
      causes at `porting/TestAudit/llvm-16.0.0/` (target **22/22 PASS, rc=0**;
      any opaque-IR failure root-caused and documented, not suppressed)
- [ ] The three critical invariants re-verified (ABI `sizeof(Entry)=32`,
      `4096 % 32 == 0` / `-g` `file:line` slices / numbering determinism)
- [ ] `git diff 63b02e2..HEAD -- test/` = only the documented harness deltas
      (`test/Makefile.common` + `test/HelloWorld/Makefile`); no golden or
      criterion file changed
- [ ] Standalone-tool whole-result validation via `tracer` CLI + `prtrace` on
      all 22 traces + `test/HelloWorld` harness (slice lines match the
      15.0.0-run result `8 10`)
- [ ] Change data: 15→16 breaks recorded at `porting/llvm-releases/16.0.0/`
      (per-version `16.0.0-api-breakings.yaml` + consolidated
      `api-breakings.yaml` extended to 8.0.0–16.0.0; note: 16.0.0 is the
      opaque-pointer-only + C++17-mandate + legacy-PM-removed release)
- [ ] `AGENTS.md` branch copy updated (Current state + Known residuals)
- [ ] PR opened into `port/llvm-16.0.0` and linked below

## Files / scope

- `Dockerfile` (ubuntu:18.04 base, CMake pin, `install_llvm.sh 16.0.0`)
- `utils/install_llvm.sh` (16.0.0 case)
- `CMakeLists.txt` (`find_package(LLVM 16.0)`)
- `lib/Giri/`, `lib/Utility/` (15→16 C++ API fixes only; pass logic
  byte-identical)
- `test/Makefile.common`, `test/HelloWorld/Makefile` (harness only; KEEP
  `-Xclang -no-opaque-pointers` — the s1 "inert" spike note was wrong, see the
  progress-log correction — plus `-no-pie`/`-g`/`-Wno-error=…`; no harness
  change turned out to be needed for 16.0.0)
- `porting/TaskNotes/Tasks/llvm-16.0.0-port.md` (this file)
- `porting/TestAudit/llvm-16.0.0/`, `porting/llvm-releases/16.0.0/`,
  `AGENTS.md` (branch copy)
- **Not touched:** test cases, golden `ans-*.txt`, criterion files,
  `include/Giri/Runtime.h` (Entry ABI)

## Blocked by

- ~~15.0.0 new-PM port (done, PR #21; the base is its head `63b02e2`)~~

## Progress log

- 2026-08-31 13-47 **Branch cut + task note opened.** `port/llvm-16.0.0`
  created at the 15.0.0 new-PM head `63b02e2` (PR #21, still open at cut
  time) and pushed; `agent/jcode/llvm-16.0.0-port` cut from the same commit.
  Spike (measured, 2026-08-31 13-07..13-40) already done: 16.0.0 prebuilt
  (ubuntu-18.04 asset, 966,785,280 bytes) loads cleanly on `ubuntu:18.04`
  (0 missing `ldd` symbols, `GLIBCXX_3.4.21` ≤ 3.4.25 → no shim),
  `llvm-config --version` 16.0.0, `--cxxflags` `-std=c++17`, opaque-pointer
  emit only (the 15.0.0 `-Xclang -no-opaque-pointers` flag is gone — accepted
  but inert, IR comes out `ptr`), `-load`/`-load-pass-plugin`/`-passes`
  plugin mechanism intact, `--enable-new-pm` survives only as a migration aid.
  TODO 1/9 done; next: toolchain wiring commit.  (continued) **Toolchain wiring** (this commit): `utils/install_llvm.sh` gained a `"16.0.0"` case (ubuntu-18.04 GitHub-Releases asset, 966,785,280 bytes; the 18.04 base image is required because the prebuilt links libtinfo.so.5, absent from 20.04; GLIBCXX_3.4.21 <= 18.04's 3.4.25 so no shim); `Dockerfile` rewritten (FROM ubuntu:18.04, CMake 3.31.12 pin retained — the 16.0.0 prebuilt's LLVMConfigVersion.cmake sets no CMake floor, the project's 3.5 floor is the binding constraint, and 3.31.12 satisfies 16.0.0's soft 3.20 requirement); `CMakeLists.txt` `find_package(LLVM 15.0 -> 16.0)`.
  - 2026-08-31 14-57 **Build fixes (s4) + s1 correction.** Three root-caused
    fixes to build Giri's new-PM passes/tools against 16.0.0: (1) `CMakeLists.txt`
    now sets `CMAKE_CXX_STANDARD 17` + `CMAKE_CXX_STANDARD_REQUIRED ON` — 16.0.0
    headers use C++17 (`std::is_integral_v` in `llvm/ADT/bit.h`) and the prebuilt's
    CMake config exports no `LLVM_OPTIMIZED_CXX_FLAGS`, so without this the
    project compiled with the host gcc's `gnu++14` default and failed; (2)
    `lib/Giri/TracingNoGiri.cpp` `getOrInsertF` default arg `ArrayRef<Type *>
    Args = None` -> `= {}` (the 3.4 `None` sentinel was removed in modern LLVM;
    the sole default-using call site `giriCtor` is a zero-arg fn, so `{}` is
    behavior-identical); (3) `tools/Tracer/CMakeLists.txt` now probes the
    `llvm-config --libfiles all` static libs at configure time and compiles
    `llvm_std_shim.cpp` **only when** one actually references
    `std::__throw_bad_array_new_length` — the 16.0.0 prebuilt's static libs
    reference it 0x (the 15.0.0 rhel-8.4 libLLVMAnalysis.a/libLLVMBitWriter.a
    did), so the shim TU is skipped; hard-coding it on would fail because
    18.04's libstdc++ lacks the `_GLIBCXX_NODISCARD` macro. Build is green
    (rc=0, all 5 artifacts: libgiri.so/libdgutility.so/librtgiri.a in build/lib,
    tracer/prtrace in build/bin) and the honest `TEST_PARALLELISM=seq` suite
    passes **22/22** against the pristine 3.4 goldens. **CORRECTION to the s1
    entry above:** the spike's claim that `-Xclang -no-opaque-pointers` is
    "accepted but inert, IR comes out ptr" was **wrong on this exact prebuilt**.
    The 16.0.0 ubuntu-18.04 clang honors the harness's `-Xclang
    -no-opaque-pointers` passthrough form and it is **load-bearing**: with it
    the IR is *typed* (`i32*`, `i32**`) — matching the 3.4 goldens and why the
    suite passes 22/22; without it clang 16 emits *opaque* `ptr` and the kmeans
    golden would drift (the very drift the 15.0.0 port measured). The flag
    therefore stays in `test/Makefile.common`; no harness change was needed for
    16.0.0, so `git diff 63b02e2..HEAD -- test/` is empty (stronger than the
    DoD's "limited to 2 harness files").
  - 2026-08-31 19-30 **s5/s6 verified in-container** (container `giri16`;
    LLVM 16.0.0 at `/usr/local/llvm`): `git diff 63b02e2..HEAD -- test/`
    empty (no harness change needed); `include/Giri/Runtime.h` identical to
    base (`git diff fba2565..HEAD -- include/Giri/Runtime.h` empty);
    `-no-pie` / `-g` / `-Wno-error=…` / `-no-opaque-pointers` intact in
    `test/Makefile.common`. Invariants re-derived in-container:
    `sizeof(Entry)=32` on x86_64 LP64, `4096 % 32 == 0`, every fresh
    `.trace` record-size `%32==0` (22/22), numbering determinism by
    construction (identical `function(mergereturn),bbnum,lsnum,…,remove-bbnum,remove-lsnum`
    pipeline in both the instrument and slice stages, `test/Makefile.common`);
    behaviorally proven by the 22/22 seq suite against the pristine 3.4
    goldens.
  - 2026-08-31 20-15 **s7 standalone-tool validation: 22 PASS / 0 FAIL.**
    Reused the path-agnostic 15.0.0 `_tool_validation/full_tool_validation.py`
    (saved under `porting/TestAudit/llvm-16.0.0/_tool_validation/`): every
    `tracer` run's slice matches the 3.4 golden, `prtrace` decodes all 22
    traces (Entry ABI), and the kmeans slice matches its 2-loc golden (no
    opaque-pointer drift).
  - 2026-08-31 21-05 **s8 cold-build acceptance PASSED:** `/giri/build`
    wiped + `source /giri/utils/build.sh` (from `/giri`) → rc=0, 22 PASS,
    all 5 artifacts. (A first attempt failed rc=2 because `build.sh` was
    invoked without `cd /giri` first — `GIRI_ROOT=$(pwd)` defaulted to `/`.)
    Evidence assembled under `porting/TestAudit/llvm-16.0.0/`: SUMMARY.md,
    22 per-test reports, suite_final_table.txt, _test_logs/ (23 logs),
    _tool_validation/ (22 PASS/0 FAIL), _cold_acceptance/ (cold-build suite
    log, negative control: empty slice ≠ non-empty golden, diff rc=1; toolchain
    provenance — Ubuntu 18.04.6, glibc 2.27, gcc 7.5.0, CMake 3.31.12,
    llvm-config 16.0.0 Release, clean ldd, GLIBCXX max 3.4.21 ≤ host 3.4.25,
    libtinfo.so.5 present, --cxxflags = -std=c++17, no CMake floor).
    `AGENTS.md` rewritten as the 16.0.0 branch copy (Current state + Known
    residuals — no `[regression]` rows).
  - 2026-08-31 23-00 **s8 change data.** `porting/llvm-releases/16.0.0/` set
    up: the saved `LLVM 16.0.0 Release Notes.html` (47 KB), the raw
    per-version `16.0.0-api-breakings.yaml` (86 entries extracted + triaged
    from the release notes; the 8 same-id changes re-stated in the 16.0.0
    notes reuse their pre-existing ids so they merge in place when the
    consolidated file is built), and the consolidated `api-breakings.yaml`
    extended 8.0.0 → 16.0.0 (baseVersion 8.0.0; 406 entries; 9 new-id
    16.0.0 entries in the 16.0.0 block + 8 same-id changes merged in place —
    versions[]/originalIds[] extended — with notes-16.0.0 enrichment on the 3
    port-relevant ones: the build-toolchain C++17 floor, the constant-expr
    fneg removal, and the opaque-pointer re-verification). 16.0.0 header
    facts root-caused against the prebuilt: the old `ReadNone` enum is gone
    (replaced by the `memory(...)` attribute, AttrBuilder::addMemoryAttr,
    Attributes.h:1237); `Attribute::NoUnwind` still compiles (minimal TU
    rc=0); Giri uses no removed memory attributes, no `fneg`, no
    `flt.rounds`; it uses `PointerType::getUnqual` (TracingNoGiri.cpp:94,
    TraceFile.cpp:685) and `ConstantExpr::getZExtOrBitCast`, both
    unaffected.

## Handoff

- branch `agent/jcode/llvm-16.0.0-port`
- PR: giriupdates #23 https://github.com/eliasbur/giri-updates/pull/23
Refs: `porting/AgentGuide.md`, `porting/HowItWorks.md`,
`llvm-15.0.0-port.md`, `porting/TestAudit/llvm-15.0.0/SUMMARY.md`
