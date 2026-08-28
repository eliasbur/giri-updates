---
title: Port Giri to LLVM 15.0.0 (new pass manager; new PM is the default, legacy PM still available)
status: done
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-27
# dateModified / completedDate are added automatically by `driver.py finish`
dateModified: 2026-08-28T19:13:09.908+02:00
completedDate: 2026-08-28
---
## Goal

A Giri source tree on `port/llvm-15.0.0` that builds cleanly against LLVM/Clang
15.0.0 (prebuilt-tarball toolchain), whose execution path is the **new pass
manager** — 15.0.0 made the new PM the **default** (the legacy PM is deprecated
in 14 and scheduled for removal *after* 14, but it is still present in 15.0.0:
`opt` retains `--enable-new-pm` and `-enable-new-pm=0` still works). This port
deliberately runs the forward-compatible new-PM path, which is the one that
survives LLVM 16+ where the legacy PM is actually gone — runs the full test
suite on the honest harness with new-PM `opt -passes` pipelines, with every
remaining test failure root-caused and documented, and the three critical
invariants preserved.

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
     (glibc 2.31). (15.0.0 builds with **C++14**, not C++17 — verified:
     `llvm-config --cxxflags` on the 15.0.0 toolchain is `-std=c++14`; C++17
     only becomes a hard requirement in 16.0.0.)
   - `Dockerfile`: `FROM ubuntu:20.04`; bump the pinned CMake binary as far as
     possible (the 15.0.0 source CMake floor is **3.5**; the 14.0.0 image
     pinned 3.12.4. Pin a known-good recent CMake Linux-x86_64 binary and
     record the version here; CMake 3.31.12 was chosen and is pinned).
   - `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.4.3)` → `3.5` —
     matches the LLVM 15.0.0 source CMake floor (verified from
     `llvm/llvm-project` 15.0.0 `CMakeLists.txt`). This is a Giri-side
     forward-compatibility choice, **not** a direct LLVM-15 mandate on
     consumers; `find_package(LLVM 15.0 REQUIRED CONFIG)` is likewise
     Giri-side (the 15.0.0 config files set their own version policy).
2. **14→15 API deltas (expected small; verify, don't assume).** The 14.0.0
   new-PM code uses no legacy-PM API (zero residual, verified). Known 14→15
   breaks that could touch Giri:
   - C++ standard: **15.0.0 still builds with C++14** (`llvm-config --cxxflags`
     = `-std=c++14`); C++17 only becomes mandatory in 16.0.0. The 15.x
     toolchain minimums are *soft* and skippable with
     `-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON`, so no Giri C++ change is
     required by the standard bump (the Giri C++ is also already clean of the
     removed constructs — no `register`, `auto_ptr`, `unary_function`,
     `random_shuffle` — grepped).
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

- [x] Spike: 15.0.0 prebuilt loads on `ubuntu:20.04` (`ldd` clean);
      `llvm-config --version` == 15.0.0; assertion-mode recorded (Release,
      assertions OFF); new-PM probe plugin + `mergereturn` + `PostDominatorTree`
      verified
- [x] Toolchain wired: `install_llvm.sh` 15.0.0 case, `Dockerfile`
      (`ubuntu:20.04` + CMake 3.31.12 pinned + `install_llvm.sh 15.0.0` +
      `DEBIAN_FRONTEND=noninteractive`), `CMakeLists.txt` `cmake_minimum_required`
      bump to 3.5
- [x] Build green in `giri-llvm-15`: 5 artifacts in `build/{lib,bin}`;
      `opt -passes=...` lists/runs the Giri passes; any 14→15 API fixes keep
      pass logic byte-identical
- [x] Honest-harness seq suite run in `giri-llvm-15`; per-test results + root
      causes at `porting/TestAudit/llvm-15.0.0/` (**22/22 PASS, rc=0**)
- [x] The three critical invariants re-verified (ABI `sizeof(Entry)=32`,
      `4096 % 32 == 0` / `-g` `file:line` slices / numbering determinism)
- [x] `git diff 72258e4..HEAD -- test/` = only the documented harness deltas
      (`test/Makefile.common` + `test/HelloWorld/Makefile`)
- [x] Standalone-tool whole-result validation: **22/22** via `tracer` CLI +
      `prtrace` on all 22 traces + `test/HelloWorld` harness (slice lines
      `8 10`, matches 14.0.0-newpm)
- [x] Change data: 14→15 breaks recorded at `porting/llvm-releases/15.0.0/`
      (per-version `15.0.0-api-breakings.yaml` 46 entries + consolidated
      `api-breakings.yaml` 397 entries + 9.0.0–15.0.0 release-notes HTMLs;
      note: 15.0.0 keeps C++14 and the legacy PM — those are 16.0.0 mandates)
- [x] `AGENTS.md` branch copy updated (Current state + Known residuals)
- [ ] PR opened into `port/llvm-15.0.0` and linked below

## Files / scope

- `Dockerfile` (ubuntu:20.04 base, CMake bump, `install_llvm.sh 15.0.0`)
- `utils/install_llvm.sh` (15.0.0 case)
- `CMakeLists.txt` (`cmake_minimum_required` bump, if the recent CMake warns)
- `lib/`, `tools/`, `include/` — **only if** the spike/build surface a 14→15
  API break in Giri code (expected minimal: the 14.0.0 new-PM code uses no
  legacy-PM API, and 15.0.0's C++14 standard is a non-change for the tree)
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

### 2026-08-27 — Port cut from the 14.0.0 new-PM head

Branch `agent/jcode/llvm-15.0.0-port` cut from `72258e4` (the 14.0.0 new-PM
head; the 9.0.0–14.0.0 change history is inherited unchanged). Task note
opened (commit `40780f3`).

### 2026-08-28 — Port complete (functional + evidence)

Commits on `72258e4` (chronological):

- `0500b26` **Toolchain wiring.** `Dockerfile` → `ubuntu:20.04` (the 15.0.0
  rhel-8.4 prebuilt needs glibc ≥ 2.28; focal provides 2.31), CMake 3.31.12
  pinned, `utils/install_llvm.sh` 15.0.0 case (the only x86_64 prebuilt is the
  `clang+llvm-15.0.0-x86_64-linux-gnu-rhel-8.4.tar.xz`; verified against the
  GitHub API), `CMakeLists.txt` floor `3.4.3 → 3.5` + `find_package(LLVM 15.0
  REQUIRED CONFIG)`. The 3.5 floor is a Giri-side forward-compat choice, not a
  direct LLVM-15 mandate on consumers (the 15.0.0 source CMakeLists floor is
  3.5; the prebuilt's `LLVMConfigVersion.cmake` sets no CMake version floor).
- `6f7954d` **`tracer` link shim** (`tools/Tracer/llvm_std_shim.cpp`). The
  prebuilt rhel-8.4 LLVM libs reference `std::__throw_bad_array_new_length`
  (GLIBCXX 3.4.29); focal's libstdc++ 6.0.28 predates that out-of-line helper.
  The shim (`llvm_std_shim.cpp`, 3-line function body) supplies it so the tracer
  links.
- `31e36fe` **Harness parity fixes** (`test/Makefile.common`,
  `test/HelloWorld/Makefile`): `-Xclang -no-opaque-pointers` (the 15.0.0
  clang driver does not expose `-opaque-pointers` directly), `-fPIE`/`-no-pie`
  handling, and `-Wno-error=implicit-function-declaration` (15.0.0's clang
  errors on implicit function declarations; the 3.4-vintage `even.c` relies on
  them). No test case/golden/criterion file touched.
- `847a130` **Tracer `DataLayout` self-assignment removed**
  (`tools/Tracer/Tracer.cpp`). `M->setDataLayout(M->getDataLayout())` is a
  self-assignment through the **hand-written** `DataLayout::operator=`
  (`llvm/IR/DataLayout.h:213`, byte-identical 14.0.0/15.0.0), which calls
  `clear()` then copies members from the (cleared) same object → corrupts the
  layout. Latent in ≤14.0.0; the 15.0.0 verifier's
  `DataLayout::ParamMaxAlignment = 1 << 14` check aborts the standalone tracer
  on it. Removed (the no-op it is). Commit message corrected to this verified
  mechanism (not "defaulted/memberwise").
- `56d0d4c` **Change data** at `porting/llvm-releases/15.0.0/`: per-version
  `15.0.0-api-breakings.yaml` (46 entries, matching the 14.0.0 per-version
  scope; 5 redacted markers: top-level list, AVR, Hexagon, MIPS, WebAssembly;
  1 affected release-note entry `llvm-ir-opaque-pointers-default`, fixed via
  `-Xclang -no-opaque-pointers`) + consolidated `api-breakings.yaml` (354 →
  397 entries: 39 new 15.0.0 release-note entries, 7 colliding ids extended
  with `15.0.0`, 4 new header-level port-critical entries) + all 9.0.0–15.0.0
  LLVM release-notes HTML provenance.
- `227c008` **Dockerfile `DEBIAN_FRONTEND=noninteractive`** (focal's
  tzdata/apt prompt) + corrected `Tracer.cpp` comment documenting the
  hand-written `operator=` mechanism.
- `51fa746` **TestAudit evidence** at `porting/TestAudit/llvm-15.0.0/`: raw
  suite logs (`_test_logs/`, `suite_final_table.txt` = 22 PASS / 0 FAIL),
  standalone-tracer validation (`_tool_validation/`, 22 PASS / 0 FAIL), 22
  per-test reports, and `SUMMARY.md`.
- `32fbe54` **Task-note corrections.** Fixed the two wrong opening premises
  (legacy PM is still present in 15.0.0; 15.0.0 is C++14, C++17 is a 16.0.0
  mandate; the CMake 3.5 floor + `find_package(LLVM 15.0)` are Giri-side
  choices), checked the DoD with the verified results, filled the progress
  log, and corrected the Dockerfile CMake comment (the prebuilt's
  `LLVMConfigVersion.cmake` sets no CMake version floor).
- `1ba5b47` **AGENTS.md branch copy.** Rewrote `## Current state` +
  `## Known residuals` for the 15.0.0 port (corrected legacy-PM/C++14
  narrative, DataLayout root cause, the `opt -stats` stderr difference,
  HelloWorld hand-run `8 10`); the shared tail (`## Containers` onward) kept
  byte-identical to the inherited copy.
- `3f20a7c` **`driver.py finish`.** Status `done`, `completedDate`, Handoff
  line `PR: giriupdates #21`; opened PR #21 into `port/llvm-15.0.0`.

**Results (verified in the `giri15` container):**

- Suite: **22 PASS / 0 FAIL** (rc=0) — 19 UnitTests (test1–5, test8–21) + 3
  benchmark seq (`matrix_multiply`, `pca`, `kmeans`), all against the pristine
  3.4 goldens. `suite_final_table.txt` = 22 `[PASS]` rows.
- Standalone `tracer` (the new-PM programmatic-pipeline path, hand-built MAM):
  **22 PASS / 0 FAIL** (`full-tool-validation.txt`); `prtrace` OK on all 22
  traces. The DataLayout abort is gone.
- `test/HelloWorld` hand-run (`make -C test/HelloWorld all`): full new-PM
  pipeline prints slice lines `8 10` (matches 14.0.0-newpm).
- Invariants: `git diff 72258e4..HEAD -- include/Giri/Runtime.h` empty;
  `sizeof(Entry)=32`, `4096 % 32 == 0` re-checked in-container; `-g`
  debug-info slicing and numbering determinism hold (22/22 against 3.4
  goldens).
- `llvm-config --version` = 15.0.0; `--cxxflags` = `-std=c++14` (**not** C++17
  — C++17 becomes hard in 16.0.0); the 15.x toolchain minimums are soft
  (`-DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON`).
- Legacy PM **still present** in 15.0.0: `opt` retains `--enable-new-pm` and
  `-enable-new-pm=0` works; the new PM is the default. This port runs the
  forward-compatible new-PM path (the one that survives 16+ where the legacy
  PM is actually gone). **The task-note's two opening premises — "legacy PM is
  gone in 15" and "LLVM 15 requires C++17" — were wrong and have been
  corrected here; do not propagate them into AGENTS.md or the change data.**
- **Cold-build + negative-control acceptance** (follow-up, 2026-08-28,
  `_cold_acceptance/`): a **from-scratch** build of the committed source at
  HEAD (`/giri/build` wiped, `source utils/build.sh`) → build.sh exit 0,
  **22/22 PASS**; the standalone `tracer` re-run from the cold build →
  **22/22**. Negative control: with the test1 trace file *removed*, the
  `dgiri` slice aborts (opt rc 139, SIGSEGV in the `DynamicGiri`/`TraceFile`
  path; the `(fd > 0)` assert at `TraceFile.cpp:52` is compiled out in
  Release) and **no `.slice` is produced**, so a golden match is impossible
  without a genuinely written+read trace. Restoring the trace reproduces the
  4-line golden match (exit 0). This mirrors the 14.0.0-newpm negative
  control (`9a2a29e`).
- `e80419b` + `b179406` + `1df52e3` **doc tweaks** (post-PR): tightened the
  link-shim wording (a 3-line *function body*, not a 3-line file) in
  SUMMARY/AGENTS.md/task note; committed the negative-control raw transcript
  (`_cold_acceptance/negative-control.log`); and replaced the one-file sync
  shorthand in the cold-acceptance README with the actual verification — a
  full-tree md5 manifest (all 483 committed git blobs vs on-disk hashes in
  `giri15`): 480/483 byte-identical at cold-build time (only drift: 3
  docs/config files the build never reads — `Dockerfile` comment-only,
  `AGENTS.md`, this note), every build input already byte-identical (drift
  0); after syncing those 3 docs, 483/483, 0 missing, 0 mismatch. Every
  commit on the branch has an entry.
- `opt -stats` stderr difference: 15.0.0 prebuilt `opt` prints nothing for
  `-stats`; 14.0.0 prebuilt `opt` prints
  `Statistics are disabled.  Build with asserts or with -DLLVM_FORCE_ENABLE_STATS`
  (44× in the 14.0.0-newpm suite logs, 0× in 15.0.0). Harmless (stderr only).

**Remaining:** none — `AGENTS.md` updated, branch pushed, PR #21 open into
`port/llvm-15.0.0`, `driver.py finish` run (status `done`). Cold-build +
negative-control acceptance added as a follow-up (see `_cold_acceptance/`).

**Container:** `giri15` (image `giri-llvm-15`; ubuntu:20.04 + prebuilt
rhel-8.4 LLVM/Clang 15.0.0 at `/usr/local/llvm`). Rebuild/test inside it:
`source /giri/utils/build.sh`. Build output flat: `build/lib`
(`libgiri.so`, `libdgutility.so`, `librtgiri.a`) + `build/bin`
(`tracer`, `prtrace`).

## Handoff

- branch `agent/jcode/llvm-15.0.0-port`
- PR: giriupdates #21 https://github.com/eliasbur/giri-updates/pull/21
Refs: `porting/AgentGuide.md`, `porting/HowItWorks.md`, `llvm-14-newpm-port.md`,
`porting/TestAudit/llvm-14.0.0-newpm/SUMMARY.md`
