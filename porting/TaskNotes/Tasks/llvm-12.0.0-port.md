---
title: Port Giri to LLVM 12.0.0 (legacy pass manager line).
status: open
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []             # e.g. dev, cockpit, gpu1
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-09-04
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal
A Giri source tree on `port/llvm-12.0.0` that builds cleanly against prebuilt
LLVM/Clang 12.0.0 inside its Docker image and passes the full honest seq test
suite against the pristine 3.4 goldens, with the standalone `tracer`/`prtrace`
tools validated, the three critical invariants preserved, and the delivery
evidence (TestAudit + change data + branch AGENTS.md) committed.

## Approach

**Lineage.** 12.0.0 sits on the **legacy pass manager** line: 5.0.2 → 8.0.0 →
**12.0.0** → 14.0.0-legacypm (PR #19). Base: the completed 8.0.0 legacy-PM port
head `224bdfb` (remote ref `port/llvm-14.0.0-legacypm`@224bdfb; the remote
`port/llvm-8.0.0` ref at `5527588` is the stale pre-port 5.0.2 base). 12.0.0 uses
the legacy PM by default (no `-enable-new-pm` flag needed at all) and typed
pointers by default (pre-opaque-pointer era), so the harness is expected to need
**zero** changes: `git diff 224bdfb..HEAD -- test/` must be empty.

**Toolchain (approved deviation).** The `llvmorg-12.0.0` GitHub release ships no
x86_64 ubuntu-18.04 asset (x86_64 assets: ubuntu-16.04, ubuntu-20.04, sles12.4),
so the 18.04 convention used by the 8/14/15/16 images cannot apply. Use
**ubuntu:20.04** + `clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz`
(453,059,268 bytes, asset id 35574458) at `/usr/local/llvm`, keeping the cmake
3.12.4 binary pin. Fallback (last resort): ubuntu:16.04 + 16.04 tarball, which
requires old-releases repo redirection.

**Predicted source fixes** (subset of the 8→14-legacypm delta `224bdfb..fba2565`;
13/14-only items excluded — `sys::fs::F_*`→`OF_*` rename is 13.0.0, the
DEBUG_TYPE-after-includes move and the `Twine` fix are 14.0.0-era):
1. `FunctionCallee` (9.0.0): `getOrInsertFunction(...)` returns `FunctionCallee`
   → `.getCallee()` + `cast<Function>` in `include/Utility/Utils.h` (2 sites) and
   `lib/Giri/TracingNoGiri.cpp` (1 site).
2. `CallSite` removal (11.0.0): `lib/Giri/TraceFile.cpp` (`const CallBase *`,
   `arg_size()`/`getArgOperand(i)`; drop the 8.0.0 `DEBUG(X)` shim if 12's
   `Debug.h` still provides bare `DEBUG`), `lib/Giri/TracingNoGiri.cpp` +
   `lib/Utility/SourceLineMapping.cpp` (`CallBase::getCalledOperand()`).
3. `BasicBlockPass` deleted (10.0.0): `TracingNoGiri` → `FunctionPass` with a
   `runOnFunction` loop calling `runOnBasicBlock` per BB
   (`include/Giri/Giri.h`, `lib/Giri/TracingNoGiri.cpp`).
4. Contingent: `#include <map>` in `include/Utility/LoadStoreNumbering.h`
   (the 14-legacypm port added it; whether 12.0.0 headers make it load-bearing
   is confirmed by the compiler).
Each fix is applied only when the 12.0.0 compiler demands it, root-caused, and
committed separately. If a fix turns out unnecessary on 12.0.0, record why in
the Progress log.

**Invariants** (must hold after the port): numbering determinism (identical
`-mergereturn -bbnum -lsnum … -remove-bbnum -remove-lsnum` sequence in both
pipeline stages, `test/Makefile.common`), the `Entry` struct ABI in
`include/Giri/Runtime.h` (untouched; `sizeof(Entry)`=32, divides the 4096 page
size), and debug-info-driven source line mapping (`-g` mandatory).

## DoD

- [ ] `port/llvm-12.0.0` created from `224bdfb`; working branch
      `agent/jcode/llvm-12.0.0-port`
- [ ] Image `giri-llvm-12` builds (ubuntu:20.04 + 12.0.0 prebuilt, provenance
      documented in the Dockerfile comment); `source /giri/utils/build.sh`
      produces `build/lib/{libgiri.so,libdgutility.so,librtgiri.a}` and
      `build/bin/{tracer,prtrace}`
- [ ] `opt -load` lists `-bbnum`, `-lsnum`, `-trace-giri`, `-dgiri`
- [ ] Honest seq suite (`TEST_PARALLELISM=seq`): **22 PASS / 0 FAIL** against the
      pristine 3.4 goldens; any FAIL root-caused (criterion-drift policy:
      documented FAIL-EXPECTED residual; retuning needs explicit user consent)
- [ ] Standalone `tracer` + `prtrace` validated over the same 22 cases
- [ ] Cold-build acceptance: wipe `/giri/build`, `source /giri/utils/build.sh`
      from `/giri` → rc=0, 22 PASS, 5 artifacts; negative control (empty slice
      ≠ golden)
- [ ] In-container invariants re-derived (`sizeof(Entry)`, trace `%32`, pipeline
      identity, `file:line` mapping)
- [ ] `git diff 224bdfb..HEAD -- test/` and `-- include/Giri/Runtime.h` empty
- [ ] Evidence: `porting/TestAudit/llvm-12.0.0/` (SUMMARY + 22 per-test reports +
      `_test_logs/` + `_tool_validation/` + `_cold_acceptance/`)
- [ ] Change data: `porting/llvm-releases/12.0.0/` (raw 9.0.0–12.0.0 yamls +
      release-notes HTML + consolidated `api-breakings.yaml` 8.0.0→12.0.0 with
      port-critical `originalIds: []` entries)
- [ ] `AGENTS.md` rewritten as the 12.0.0 branch copy (Current state + Known
      residuals, no `[regression]` rows)
- [ ] PR opened into `port/llvm-12.0.0` via `driver.py open-mr`;
      `driver.py finish` (human merge only)

## Progress

### 2026-09-04 — plan approved
Plan approved by the user (s1–s9). Grounding: 8→14-legacypm delta
(`224bdfb..fba2565`) captured as the prediction set; 12.0.0 prebuilt asset
verified on the GitHub release (no 18.04 x86_64 asset → 20.04 base); 9.0.0–
12.0.0 raw yamls + release-notes HTML already present in
`porting/llvm-releases/{14.0.0,16.0.0}/` for copying.

### 2026-09-04 — s1 spike PASS
Image `giri12-spike` (ubuntu:20.04 + 12.0.0 20.04 prebuilt, cmake 3.12.4 pin).
Probes (in the spike's build log):
- `llvm-config --version` == **12.0.0**; `clang --version` == clang 12.0.0.
- `ldd` on `opt` and `clang`: **LDD CLEAN** (no missing libs).
- `llvm-config --cxxflags` → `-std=c++14 -fno-exceptions -fno-rtti …`.
  Host gcc 9.4.0's default is `gnu++14`, matching the 12.0.0 headers, so
  **no forced `CMAKE_CXX_STANDARD` pin is expected** (the 16.0.0 pin was
  needed because 16.0.0's headers are C++17 and the prebuilt config exported
  no standard; 12.0.0's prebuilt CMake config likewise exports no
  `CMAKE_CXX_STANDARD`/`LLVM_OPTIMIZED_CXX_FLAGS`, but the host default is
  already C++14, so the 8.0.0 CMake flags stay untouched).
- GLIBCXX `__throw_bad_array_new_length` symbol probe over
  `llvm-config --libfiles all` static libs: **NOT REFERENCED** → the Tracer
  shim TU is not needed (and 20.04's libstdc++ would have it anyway).
- Hello-world legacy-PM `FunctionPass` compiled with the tree flags
  (`llvm-config --cxxflags` + `-fno-rtti -fno-exceptions`, `-fPIC`) and
  `opt -load … -hello` ran it: **SPIKE-PASS**.
- Toolchain provenance: ubuntu:20.04 (gcc 9.4.0), prebuilt
  `clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz`
  (453,059,268 bytes, GitHub Releases asset 35574458), at `/usr/local/llvm`.

**Deviation note (toolchain):** the 12.0.0 GitHub release has **no x86_64
ubuntu-18.04 asset** (x86_64 assets: ubuntu-16.04, ubuntu-20.04, sles12.4),
so the image base moves 18.04 → 20.04 (matching the tarball). This is a
documented deviation from the 18.04 convention of the 8/14 images (the 15/16
ports used 20.04/18.04 respectively per their prebuilt assets).

### 2026-09-04 — s2 branches + task note
`port/llvm-12.0.0` created from the 8.0.0 head `224bdfb`; working branch
`agent/jcode/llvm-12.0.0-port`. Task note committed (`aeb18f6`).

### 2026-09-04 — s3 toolchain wiring
- `Dockerfile` → `ubuntu:20.04` + `DEBIAN_FRONTEND=noninteractive` (on 20.04
  `apt-get upgrade` pulls tzdata and otherwise blocks on the geographic-area
  prompt without `TERM`) + `install_llvm.sh 12.0.0`; provenance comment
  documents the no-18.04-asset reason.
- `utils/install_llvm.sh` → new `12.0.0` case (GitHub-Releases 20.04 tarball).
- `CMakeLists.txt` → `find_package(LLVM 12.0 REQUIRED CONFIG)`.
- `.dockerignore` already present at the 8.0.0 head (excludes `.env`/
  `.devcontainer/jcode/`) — no change needed; verified it covers the token
  file so `ADD . giri` does not bake credentials into the image.
- Full `giri-llvm-12` image build in progress.

### 2026-09-04 — s4 source API fixes (committed f6b39bf; MAKE RC=0, 5 artifacts)
Root-caused against the 12.0.0 prebuilt (8→14-legacypm delta minus the
13/14-only items — no `F_*`→`OF_*` rename, no DEBUG_TYPE move, no Twine fix):
1. **`FunctionCallee` (9.0.0)** — `getOrInsertFunction(...)` returns
   `FunctionCallee`; `cast<Function>(...getCallee())` in
   `include/Utility/Utils.h` (2 sites) + `lib/Giri/TracingNoGiri.cpp` (1).
2. **`CallSite` removal (11.0.0)** — `lib/Giri/TraceFile.cpp` casts
   `const CallBase *` and uses `arg_size()`/`getArgOperand(i)` (16 sites);
   `TracingNoGiri.cpp` + `lib/Utility/SourceLineMapping.cpp` use
   `CallBase::getCalledOperand()`; three `llvm/IR/CallSite.h` includes
   dropped. The 8.0.0 `DEBUG(X)` shim **stays**: 12.0.0 `Debug.h` still has
   no bare `DEBUG` (only `DEBUG_WITH_TYPE`), same as 8.0.0.
3. **`BasicBlockPass` deleted (10.0.0)** — `TracingNoGiri` → `FunctionPass`
   with a `runOnFunction` loop calling `runOnBasicBlock` per BB
   (`include/Giri/Giri.h`, `lib/Giri/TracingNoGiri.cpp`).
4. **Tracer link** — 12.0.0 prebuilt CMake config sets no
   `LLVM_COMPONENT_LIBS` (same as 14-legacypm), so
   `llvm_map_components_to_libnames(tracer, all)` expands empty → undefined
   `llvm::DisableABIBreakingChecks`. Fixed with `llvm-config --libfiles all` +
   `--system-libs` (155 libs incl. `libLLVMWindowsManifest.a` holding the
   symbol). This matches the 14-legacypm `tools/Tracer/CMakeLists.txt` fix
   (confirmed via the `224bdfb..fba2565` delta).
Not needed on 12.0.0 (recorded per plan): the `#include <map>` contingency in
`LoadStoreNumbering.h` (compiles without it), the `F_*`→`OF_*` rename
(`sys::fs::F_*` is still the API in 12.0.0), the `F_None`→`OF_None` in
`tools/Tracer/Tracer.cpp` (same, still `F_None`), the DEBUG_TYPE-after-
includes move, the `Twine` fix (all 13/14-era).
Result: `source /giri/utils/build.sh` → MAKE RC=0, `build/lib/{libgiri.so,
libdgutility.so, librtgiri.a}` + `build/bin/{tracer, prtrace}`;
`opt -load` lists `-bbnum`, `-lsnum`, `-trace-giri`, `-dgiri`.

### 2026-09-04 — s5 honest seq suite + invariants (22/22)
`make -C /giri/test TEST_PARALLELISM=seq` → **22 PASS / 0 FAIL** (rc=0) vs the
pristine 3.4 goldens (19 unit tests + 3 app benchmarks). Strongest parity
clause holds: `git diff 224bdfb..HEAD -- test/` **and** `-- include/Giri/
Runtime.h` are **both empty** (zero harness/golden/criterion changes — the
8.0.0 harness works on 12.0.0 as-is; 12.0.0 predates opaque pointers and the
new-PM default, and `-constantprop` — removed from the pass set in
12.0.0 — was never used by the harness). PR range = exactly the 10 expected
files. In-container invariants re-derived (logged to `/giri/invariants.log`):
`sizeof(Entry)=32`, `4096 % 32 == 0`; fresh test1 trace = 1216 B (38×32,
`%32==0`); numbering determinism (two `-bbnum -query-bbnum` runs identical);
`-g` file:line mapping present in slices.

### 2026-09-04 — s6 standalone `tracer`/`prtrace` validation (honest scope)
**Root-caused finding:** the legacy-PM `tools/Tracer/Tracer.cpp` (the original
3.4 tool; **untouched by this port** — `git diff 224bdfb..HEAD -- tools/Tracer/
Tracer.cpp` is empty) has a *fixed* pipeline in `main()`:
`if (DoTrace) { PM.add(TracingNoGiri) } else if (DynamicGiri) { PM.add
(DynamicGiri) }` — it never adds `-bbnum -lsnum` (the `opt` harness does).
`DynamicGiri` → `TraceFile::findPreviousID` reads the numbering maps via
`QueryBasicBlockNumbers`/`QueryLoadStoreNumbers`, so running the standalone
tracer's **slice** mode SIGBUSes (walks invalid numbering). This is a
**pre-existing legacy-PM-line limitation, not a 12.0.0 regression**: the
8.0.0→14-legacypm delta (`224bdfb..fba2565`) shows the next legacy port made no
`Tracer.cpp` pipeline change either (only `F_None`→`OF_None` + the
`llvm-config --libfiles` link fix), and the legacy line has never validated
standalone-tracer slicing — the slice *result* is validated via `opt` in s5.
**Validated (standalone-able scope): 22 PASS / 0 FAIL** over the same 22 cases —
standalone `tracer -trace` instrument → `llc` → link `-lrtgiri` → run the real
program (expected exit code, 0 Abnormal-termination) → trace non-empty and
`%32==0` → `prtrace` decodes it (`End` record ⇒ Entry ABI intact). Cross-check
on test1: the standalone-`tracer` trace is **identical** (prtrace ID/Type
sequence diff empty) to the `opt` harness's `-trace-giri` trace. Evidence:
`_tool_validation/` (script + 22-case log + test1 cross-check log).
