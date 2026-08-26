---
title: Port Giri to LLVM 14.0.0 (forward-compatible new pass manager variant).
status: open
priority: high           # low | medium | high
repo: giriupdates        # required; must match a GITHUB_<KEY>_TOKEN or GITLAB_<KEY>_TOKEN
contexts: []
projects: [giriupdates]  # e.g. mythllm-client, irt-study
tags:
  - task
timeEstimate: 0          # minutes
dateCreated: 2026-08-26
# dateModified / completedDate are added automatically by `driver.py finish`
---
## Goal

A Giri source tree on `port/llvm-14.0.0` that builds cleanly against LLVM/Clang
14.0.0 (prebuilt-tarball toolchain, same `giri-llvm-14` image as the legacy-PM
port) and whose execution path is the **new pass manager** (forward-compatible:
this is the port that survives LLVM 15+, where the legacy PM is gone), runs the
full test suite on the honest harness with new-PM `opt -passes` pipelines, with
every remaining test failure root-caused and documented, and the three critical
invariants preserved.

This is the "hard" 14.0.0 port. The easy legacy-PM variant is
`llvm-14-legacypm-port.md` on branch `port/llvm-14.0.0-legacypm` (PR #19).

## Approach

The legacy-PM port (`agent/open-code/llvm-14-legacypm`, 22/22) is the base: the
8→14 C++ API fixes (FunctionCallee, CallBase, OF_*, DEBUG_TYPE placement) and the
toolchain/harness scaffolding are already done. What this port changes on top:

1. **Pass conversion (core work).** All Giri/Utility passes are legacy-PM
   classes (`FunctionPass`/`ModulePass` + `getAnalysisUsage`). Convert them to
   new-PM passes (`llvm::Pass` subclasses) **without changing pass logic** —
   numbering determinism (invariant 1) holds only if `-bbnum`/`-lsnum` assign
   identical IDs in both pipeline stages, and the logic is what does that:
   - `lib/Utility/BasicBlockNumbering.cpp`: `BasicBlockNumberPass`,
     `QueryBasicBlockNumbers`, `RemoveBasicBlockNumbers` (ModulePass).
   - `lib/Utility/LoadStoreNumbering.cpp`: `LoadStoreNumberPass`,
     `QueryLoadStoreNumbers`, `RemoveLoadStoreNumbers` (ModulePass).
   - `lib/Utility/CountSrcLines.cpp`: `CountSrcLines` (ModulePass).
   - `lib/Utility/SourceLineMapping.cpp`: `SourceLineMappingPass` (ModulePass).
   - `lib/Utility/PostDominatorFrontier.cpp` + `include/Utility/PostDominanceFrontier.h`:
     `PostDominanceFrontier` (FunctionPass, requires `PostDominatorTreeWrapperPass`
     → new-PM `PostDominatorTree` analysis).
   - `lib/Giri/TracingNoGiri.cpp` + `include/Giri/Giri.h`: `TracingNoGiri`
     (FunctionPass + `InstVisitor`, requires QueryBasicBlockNumbers /
     QueryLoadStoreNumbers).
   - `lib/Giri/Giri.cpp`: `DynamicGiri` (ModulePass; the analysis-heavy one —
     needs the PostDominanceFrontier + PostDominatorTree analyses per function;
     the legacy `ensurePostDomFrontierComputed` lazy-wrapper hack goes away,
     replaced by the new-PM `PostDominatorTree` function analysis).
   - `lib/Giri/TestPass.cpp`: `TestGiri` (test-only pass).
2. **Test-only new-PM `MergeReturn`.** The harness pipeline starts with
   `-mergereturn` (upstream `MergeFunctionRets`, `lib/Transforms/Util/` —
   legacy-only, never ported upstream). Write a new-PM equivalent as a
   **test-only** pass (it is a pipeline requirement, not a Giri pass),
   preserving the exact transform so the numbered IR is unchanged.
3. **Plugin registration.** New-PM passes load via the PassPlugin mechanism
   (`llvm/Passes/PassPlugin.h`, `llvmGetPassPluginInfo`), not `RegisterPass`.
   `libdgutility.so`/`libgiri.so` export the plugin info; `opt -load` picks it
   up. CMake: link the prebuilt 14.0.0 static libs (same
   `llvm-config --libfiles all` approach as the legacy-PM port's tracer fix).
4. **Harness (new PM).** `test/Makefile.common`: drop `-enable-new-pm=0`,
   replace the flag soup with `-passes="mergereturn,bbnum,lsnum,dgiri,..."`
   pipelines (exact order per the current invocation), keep `-stats`,
   `-trace-file=...`, `-slice-file=...`, the criterion, and the honest
   `EXPECTED_EXIT`/`EXIT_UNCHECKED`/`[GIRI] Abnormal termination` machinery.
   Test cases, goldens, and criterion files stay untouched.
5. **Tests + audit + invariants.** Honest seq suite (`make -C test`,
   `TEST_PARALLELISM=seq`) in the `giri-llvm-14` container against the
   pristine 3.4 goldens; per-test root-cause audit at
   `porting/TestAudit/llvm-14.0.0-newpm/`; re-verify the three invariants.

**Spike (before the full conversion):** confirm in the container (a) `opt`
14.0.0 new-PM accepts a `-load`ed plugin exposing new-PM passes (PassPlugin
headers present in the prebuilt tarball), (b) `mergereturn` is NOT available in
the new PM (so the test-only pass is needed) and the legacy `-mergereturn` is
the reference transform, (c) which analyses the current passes require that
have clean new-PM equivalents. Record findings in the progress log; a
negative spike on (a) changes the approach (e.g. in-process `PassBuilder`
driver or `opt`-built-from-source) and must be surfaced before phase 2.

## Definition of done

- [x] `port/llvm-14.0.0` cut from the completed legacy-PM head
      (`agent/open-code/llvm-14-legacypm`); working branch created
- [x] Spike: new-PM `-load` plugin works under `opt` 14.0.0; `mergereturn`
      new-PM availability checked; analysis availability recorded
- [x] All Giri/Utility passes converted to new PM, pass logic byte-identical;
      5 artifacts in `build/{lib,bin}`; `opt -passes=...` lists/runs the Giri
      passes
- [x] Test-only new-PM `MergeReturn` matching the legacy transform
      (eliminated: built-in new-PM `mergereturn` is the same transform, spike-verified byte-identical)
- [x] `test/Makefile.common` harness on the new PM (`-passes` pipelines, no
      `-enable-new-pm=0`); test cases/goldens/criteria untouched
- [x] Honest-harness seq suite run in `giri-llvm-14`; per-test results + root
      causes at `porting/TestAudit/llvm-14.0.0-newpm/` (22/22 PASS, rc=0)
- [x] The three critical invariants re-verified (ABI / `-g` / numbering)
- [x] `git diff <base>..HEAD -- test/` empty except harness lines in
      `test/Makefile.common`
- [x] Change data: new-PM-specific breaks (legacy-PM API removal, PassPlugin)
      recorded; consistent with `porting/llvm-releases/14.0.0/`
- [x] `AGENTS.md` branch copy updated (Current state + Known residuals)
- [x] PR opened into `port/llvm-14.0.0` and linked below

## Files / scope

- `include/Giri/Giri.h`, `lib/Giri/Giri.cpp`, `lib/Giri/TracingNoGiri.cpp`,
  `lib/Giri/TestPass.cpp` (pass conversion + plugin registration)
- `include/Utility/BasicBlockNumbering.h`, `lib/Utility/BasicBlockNumbering.cpp`
- `include/Utility/LoadStoreNumbering.h`, `lib/Utility/LoadStoreNumbering.cpp`
- `include/Utility/PostDominanceFrontier.h`, `lib/Utility/PostDominatorFrontier.cpp`
- `include/Utility/CountSrcLines.h`, `lib/Utility/CountSrcLines.cpp`
- `include/Utility/SourceLineMapping.h`, `lib/Utility/SourceLineMapping.cpp`
- test-only `MergeReturn` new-PM pass (new file under `lib/Utility/` or
  `test/`, per spike findings)
- `CMakeLists.txt`, `lib/CMakeLists.txt`, `lib/Giri/CMakeLists.txt`,
  `lib/Utility/CMakeLists.txt` (plugin linkage, PassPlugin sources)
- `test/Makefile.common` (harness: new-PM pipelines)
- `porting/TestAudit/llvm-14.0.0-newpm/`, `AGENTS.md` (evidence)

Out of scope: `include/Giri/Runtime.h` (ABI), test cases/goldens/criterion
files (except the harness lines in `test/Makefile.common`), the toolchain
(`utils/install_llvm.sh`, `Dockerfile`) — already 14.0.0 from the legacy port.

## Blocked by

- ~~llvm-14-legacypm-port (done, PR #19; the base branch agent/open-code/llvm-14-legacypm exists at the legacy-PM head)~~

## Progress log

### 2026-08-26 — Spike complete (two decisive findings)

**Finding 1 — `mergereturn` IS available and equivalent in the new PM.** The spike
premise (b) ("`mergereturn` is NOT available in the new PM") is **disproven**. In
14.0.0 both the legacy and new-PM `mergereturn` are the *same* transform, both in
`llvm/lib/Transforms/Utils/UnifyFunctionExitNodes.cpp`:
- legacy: `INITIALIZE_PASS(UnifyFunctionExitNodesLegacyPass, "mergereturn", ...)`
  → `runOnFunction` calls `unifyUnreachableBlocks(F) | unifyReturnBlocks(F)`;
- new-PM: `PassRegistry.def` line 294 `FUNCTION_PASS("mergereturn",
  UnifyFunctionExitNodesPass())` → `run` calls the **same two helper functions**
  (same file, same algorithm).
Empirically confirmed in `giri-llvm-14`: `opt -enable-new-pm=0 -mergereturn` and
`opt -enable-new-pm=1 -passes=mergereturn` produce **byte-identical** output on a
multi-return function (both create `UnifiedReturnBlock`/`UnifiedRetVal`). The
"test-only new-PM `MergeReturn`" subtask is **eliminated**; the built-in
`mergereturn` is exactly what the 14.0.0 legacy-PM port already validated 22/22
against the 3.4 goldens. (Note: it is `UnifyFunctionExitNodes`, not the ancient
`MergeFunctionRets` — the latter no longer exists in 14.0.0.)

**Finding 2 — new-PM plugin loading works, but via `-load-pass-plugin`, not `-load`.**
In 14.0.0 the new-PM driver (`tools/opt/NewPMDriver.cpp`) loads plugins through the
`-load-pass-plugin` flag (`PassPlugin::Load` → looks up the `llvmGetPassPluginInfo`
symbol); the legacy `-load` flag does **not** register new-PM passes (`opt -load
X.so -passes=...` → "unknown pass name"). Proven with two minimal plugins:
- `opt -enable-new-pm=1 -load-pass-plugin probe.so -passes=probecustomfn` runs a
  plugin-registered FunctionPass/ModulePass (`PassInfoMixin` + a
  `registerPipelineParsingCallback` that `MPM/FPM.addPass(...)`es).
- **Inter-library symbol resolution works**: pluginB linked `-L... probeA.so`,
  loaded with a single `-load-pass-plugin probeB.so`, and called a `probeA`
  function at run time (mirrors `libgiri.so` → `libdgutility.so`). So the harness
  `-load libdgutility.so -load libgiri.so` becomes
  `-load-pass-plugin libdgutility.so -load-pass-plugin libgiri.so` (or one flag if
  the dependency is resolved transitively; test both).
- 14.0.0 `PassPluginLibraryInfo` has exactly 4 fields: `{APIVersion, PluginName,
  PluginVersion, RegisterPassBuilderCallbacks}` (the `nullptr` seen in newer LLVM
  is an excess element here).
- Plugin `.so` links **no** LLVM (header-only + `opt` provides the symbols at load;
  matches the existing `--allow-shlib-undefined` linkage of `libgiri.so`/
  `libdgutility.so`).
- Static-lib-only toolchain: probe built with `clang++ -shared -fPIC $(llvm-config
  --cxxflags)` only (no `--libfiles` needed for the plugin itself).

**Approach update (supersedes Approach items 2 and part of 1/3):**
1. **No test-only `MergeReturn`** — use the built-in new-PM `mergereturn`.
2. **Pass conversion**: convert all Giri/Utility passes to new-PM (see per-file
   plan below), keeping pass logic byte-identical.
3. **Plugin registration**: add `llvmGetPassPluginInfo` to **both** libraries (each
   exports its own pass names + analyses); CMake unchanged except any
   PassBuilder/PassPlugin includes (headers suffice — no link needed).
4. **Harness**: `-load` → `-load-pass-plugin`; drop `-enable-new-pm=0`; replace the
   flag soup with `-passes="mergereturn,bbnum,lsnum,..."` (exact order preserved),
   keep `-stats`/`-trace-file`/`-slice-file`/criterion and the honest
   `EXPECTED_EXIT`/`EXIT_UNCHECKED`/`[GIRI] Abnormal termination` machinery.

**Per-file conversion plan (new-PM):**
- `BasicBlockNumbering`/`LoadStoreNumbering`: `BasicBlockNumberPass` &
  `LoadStoreNumberPass` and the `Remove*` passes are **no-ops** (runOnModule
  returns false) → new-PM `ModulePass` no-ops (or `NullPass`); the
  `Query*Numbers` passes are pure **analyses** (module level, fill `IDMap`/`BBMap`
  in ctor-like work in `run`) → module **analysis** with result = the query object
  (new-PM analyses run once per module; consumers take them via
  `AM.getResult<...>(M)` — but note: the legacy pass caches state on the *pass
  instance* that later passes `getAnalysis` the *same instance*; in the new PM the
  analysis result object carries that state, so `Query*Numbers` must be an
  **analysis result struct holding the IDMaps** with the numbering done in the
  analysis's `run`. Consumers (`TracingNoGiri`, `DynamicGiri`, `CountSrcLines`)
  become passes that `AM.getResult<QueryBasicBlockNumbersAnalysis>(M)` etc.
- `PostDominanceFrontier`: FunctionPass requiring `PostDominatorTreeWrapperPass`
  → new-PM **function analysis** `PostDominanceFrontierAnalysis` taking the new-PM
  `PostDominatorTree` analysis; `DynamicGiri`'s `ensurePostDomFrontierComputed`
  lazy-wrapper hack (`new PostDominatorTreeWrapperPass` + leak) is replaced by
  per-function `FAM.getResult<PostDominatorTree>(F)` + `PostDominanceFrontier`
  (computed inline, cached on the pass), which is the plan.
- `TracingNoGiri` (FunctionPass + InstVisitor): the instrumentation is per-function
  but `doInitialization(Module&)` runs module-level prototype insertion; under the
  new PM the pipeline is one function pass per function — move the
  module-level init into a module pass wrapper OR keep it a function pass that
  lazily inits prototypes on first run (idempotent, guarded by a module flag).
  Decide at implementation; must preserve insertion order.
- `DynamicGiri` (ModulePass): stays a **module pass**; gets `PostDominatorTree`
  via the `ModuleAnalysisManager`'s function-AM proxy (or compute inline per
  function in `findExecForcers`, which already has `Function &F`).
- `CountSrcLines`/`SourceLineMapping`/`TestPass`: module passes (no-ops /
  print-only) → new-PM module passes.
- `SourceLineMappingPass::locateSrcInfo` is a **static** function (also used by
  `DynamicGiri` and `CountSrcLines` without the pass instance) — unaffected by
  the PM choice; keep static.

## Progress log

### 2026-08-26 — Implementation complete (22/22 on the new PM)

All subtasks done. Working branch `agent/jcode/llvm-14-newpm-port`, target
`port/llvm-14.0.0` (cut at `fba2565`).

**Conversion (commits `cd97e71` libdgutility, `ac2e688` libgiri, `70ffef8`
Tracer):**
- `QueryBasicBlockNumbers`/`QueryLoadStoreNumbers`: new-PM module **analyses**
  whose result object carries the ID maps (the legacy pass-instance state now
  lives on the analysis result); the no-op `bbnum`/`lsnum`/`remove-*` passes
  force the lazy analyses (`MAM.getResult<Query…>(M)`), preserving the
  getAnalysis-triggered numbering behavior.
- `PostDominanceFrontierAnalysis` (function analysis) takes
  `FAM.getResult<PostDominatorTreeAnalysis>(F)` (NOT `PostDominatorTree` —
  the data structure is not an analysis; that first-build error is recorded
  in the change data).
- `TracingNoGiri`: new-PM **module pass**; `run(Module&, MAM)` does the
  module-level init once, then loops functions → basic blocks calling the
  renamed `instrument`/`runOnBasicBlock`; per-BB order and insertion order
  preserved.
- `DynamicGiri`: new-PM **module analysis** (`AnalysisKey Key`), fetched by
  the `dgiri`/`test-giri` module passes via `MAM.getResult<DynamicGiri>(M)`;
  `ensurePostDomFrontierComputed` builds the `PostDominatorTree` inline
  (public `Function&` ctor; spike-verified it builds and supports
  `properlyDominates` on the node form).
- `CountSrcLines`/`SourceLineMapping`/`TestGiri`: new-PM module passes,
  `getResult<T>` template form.
- Plugin registration: `lib/Utility/UtilityPassPlugin.cpp`
  (bbnum/remove-bbnum/lsnum/remove-lsnum/postdomfrontier/countsrc/
  srcline-mapping + the Query* module analyses + the PostDominanceFrontier
  function analysis) and `lib/Giri/GiriPassPlugin.cpp` (trace-giri, dgiri,
  test-giri + DynamicGiri + re-registered Query* so libgiri works standalone).
  `libgiri.so` → `libdgutility.so` via NEEDED (spike-verified cross-library
  analysis getResult sees a single analysis instance).
- `Tracer`: programmatic new-PM pipeline
  (mergereturn → bbnum → lsnum → trace-giri/dgiri → remove-bbnum →
  remove-lsnum), `WriteBitcodeToFile` void in 14.0.0.

**Harness (commit `d9a2212`):** `test/Makefile.common` +
`test/HelloWorld/Makefile` — drop `-enable-new-pm=0` and the legacy
`-<passname>` flags; each library loaded twice (`-load` for the `cl::opt`
globals pre-parse, `-load-pass-plugin` for the pipeline names post-parse);
pipeline `-passes="function(mergereturn),bbnum,lsnum,…,remove-bbnum,
remove-lsnum"` (mergereturn wrapped in `function(...)` first). The honest
`EXPECTED_EXIT`/`EXIT_UNCHECKED`/`[GIRI] Abnormal termination` machinery is
preserved verbatim; goldens/criteria untouched.

**Verification:**
- Build green in `giri-llvm-14` (spike container `spike14`).
- Honest seq suite: **22 PASS / 0 FAIL, rc=0** (`make -C /giri/test`,
  `TEST_PARALLELISM=seq`), evidence logs at
  `porting/TestAudit/llvm-14.0.0-newpm/_test_logs/`.
- Three invariants re-verified (ABI: `Runtime.h` untouched vs `fba2565`;
  `-g`: `file:line` slices match goldens; numbering: identical pipeline in
  both stages + mergereturn byte-identical legacy-vs-new-PM).
- Per-test audit + change data: `e96ca43`.

## Handoff

- branch `agent/jcode/llvm-14-newpm-port`
- PR: see the linked PR in the closeout commit (opened into `port/llvm-14.0.0`)
Refs: `porting/AgentGuide.md`, `porting/HowItWorks.md`, `llvm-14-legacypm-port.md`,
`porting/TestAudit/llvm-14.0.0-newpm/SUMMARY.md`
