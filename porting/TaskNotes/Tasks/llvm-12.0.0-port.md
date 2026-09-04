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
