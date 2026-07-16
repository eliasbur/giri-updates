# Porting Giri from LLVM/Clang 3.4 to LLVM/Clang 8.0

This document is a working plan for moving Giri off LLVM 3.4 onto LLVM/Clang 8.0. It's based on a
concrete audit of this codebase's actual API/build-system usage (not a generic LLVM changelog), and
is meant to be executed roughly phase by phase, in order, with the existing test suite used as a
regression harness at every step. See `CLAUDE.md` for the general codebase orientation this plan
assumes.

**Pinned target version:** `llvmorg-8.0.0`. This branch (`port/llvm-8.0.0`) targets exactly LLVM
8.0.0 — one branch per LLVM version — so use the `8.0.0` tag everywhere below.

**Status on this branch (foundation landed).** Phases 1–3 are **done and verified to work**:
- **Phase 1 (CMake):** verified working. The CMake build system is now in place. Critical fix:
  `utils/build.sh` uses `mkdir -p build && cd build && cmake -DLLVM_DIR=$LLVM_DIR .. &&
  cmake --build . -- -j$(nproc) && cd ..` to remain compatible with CMake 3.10 (Ubuntu 18.04's
  default); the modern `-S` and `-B` flags require CMake 3.13+. The Docker build
  (`docker build -t giri-llvm8 .`) now successfully configures and builds past Phase 2 errors.
- **Phase 2 (mechanical renames):** done. Header-path fixes and LLVM API renames applied.
- **Phase 3 (DataLayout):** done. `DataLayout` is no longer treated as a pass.

Build and test happen only inside Docker — `docker build -t giri-llvm8 .` (Ubuntu 18.04 + prebuilt
LLVM 8.0.0); Giri is never built on the host. Phase 0 (spike) and Phases 4–6 (metadata ID encoding,
debug-info line mapping, `PostDominanceFrontier`) are **not yet done** — the CMake build compiles past
the mechanical sites and then fails at those three, which is the intended handoff point.

A few phase notes below were found stale during the audit; corrected here once: the debug metadata key
is `"dbg"` (not `"Loc"`); the numbering code uses `MDNode::getWhenValsUnresolved` (not `MDNode::get`);
`PostDominanceFrontier` currently subclasses `DominanceFrontierBase` (two-arg ctor), with
`ForwardDominanceFrontierBase`/the templated `DominanceFrontierBase<BasicBlock,true>` being the LLVM 8
*target*, not the present code; and every `DataLayout` include was already `llvm/IR/DataLayout.h`.

## Obtaining LLVM 8 (prerequisite for every phase below)

This machine does not have LLVM 8 installed (checked: apt has LLVM 11/14/15 only), and Ubuntu 22.04
(jammy)'s repos don't carry an `llvm-8` package at all — so `apt install llvm-8` will fail and
shouldn't be attempted. Two different needs, two different amounts of LLVM required:

- **Phase 0 only needs to *read* two header files**, not build or link against LLVM. Fetch them
  directly instead of cloning/building anything:
  ```bash
  curl -sL https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-8.0.0/llvm/include/llvm/Analysis/DominanceFrontier.h
  curl -sL https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-8.0.0/llvm/include/llvm/Analysis/DominatorInternals.h
  ```
  (Confirmed reachable from this environment.) If `DominatorInternals.h` 404s, that itself is
  evidence it was removed by 8.0.0 — treat a 404 as a "removed" finding for Phase 0, not as a fetch
  error to retry.
- **Phase 1 onward needs an actual LLVM 8 toolchain** (headers, libs, `llvm-config`, `clang`) to
  build and test Giri against. Two options, in preference order:
  1. **Prebuilt binary release** (fast, no compile time): LLVM's GitHub releases publish
     `clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz` under
     `https://github.com/llvm/llvm-project/releases/tag/llvmorg-8.0.0` (or the older
     `releases.llvm.org/8.0.0/` mirror). The Ubuntu 18.04 build runs fine on 22.04. Prefer this —
     it sidesteps building LLVM from source entirely.
  2. **Build from source** (only if the prebuilt binary doesn't work in-sandbox): `git clone
     --branch llvmorg-8.0.0 --depth 1 https://github.com/llvm/llvm-project.git`, then a CMake build
     of the `llvm` subdirectory with `clang` in `LLVM_ENABLE_PROJECTS`. Budget for this being heavy:
     a full build is on the order of tens of GB of build artifacts and can run well past an hour even
     with `-j8`. Kick it off with `run_in_background: true` (or equivalent) rather than a synchronous
     call that will hit the tool execution timeout — do not loop retrying a timed-out build.

## Goal and non-goals

**Goal:** Giri builds against a stock, CMake-built LLVM/Clang 8.0.x and passes the existing test
suite (`make -C test test`, i.e. `test/auto-tests.txt`: 19 `UnitTests/*` cases + `matrix_multiply`,
`pca`, `kmeans`) with unchanged (or intentionally-updated-and-justified) `ans-*.txt` golden output.

**Non-goals for this pass** (explicitly deferred, since LLVM 8 doesn't require them):
- Migrating to the new (non-legacy) PassManager. LLVM 8's `opt` still defaults to the legacy PM;
  `ModulePass`/`BasicBlockPass`/`InstVisitor` keep working unchanged.
- Opaque pointers. LLVM 8 still has typed pointers; nothing in `TraceFile.cpp`/`Giri.cpp` needs to
  change on this axis. (This is a large part of why 8.0 is a meaningfully easier target than 14+.)
- Modernizing code style beyond what's needed to compile (no gratuitous `auto`/range-for rewrites,
  no unrelated cleanup). Keep diffs scoped to what LLVM 8 actually requires.

## Guiding principles

1. **Verify before rewriting.** Where I'm not sure an LLVM 8 API/class still exists (see Phase 0),
   confirm against real LLVM 8 headers before committing to a design, rather than guessing and
   discovering it mid-refactor.
2. **No incremental version crawl.** Don't step through 3.5 → 3.6 → ... → 8. Jump straight to 8 and
   fix what breaks; the codebase is small enough (~3,200 lines in `lib/`) that this is faster than a
   15-release crawl.
3. **Test suite is the regression oracle.** After every phase that can plausibly change slice output
   (Phases 3-6), run the full test suite and confirm `ans-*.txt`/`criterion-*.txt` diffs are clean
   before moving on. Don't stack multiple semantically-risky changes before checking.
4. **Land the build system before touching C++.** Nothing else is checkable until Giri can invoke a
   compiler against LLVM 8 headers at all.

## Current compilation errors (Phase 4-6, expected and documented here for next session)

When running `docker build -t giri-llvm8 .` with the current codebase, the build reaches the Phase 4-6
code and fails with these error categories (documented for reference in the next session):

**Phase 4 (Metadata/Value split):**
- `BasicBlock::iterator` / `Instruction*` conversion errors: functions like `skipAllocas()` return an
  iterator but callers expect a raw pointer. LLVM 8 separates iterators from pointers; code needs to
  dereference or call `&*iterator`.
- `MDNode::getWhenValsUnresolved` no longer exists; needs `ConstantAsMetadata`-wrapped approach
  per Phase 4 action items.
- Type mismatches on iterator arguments to functions expecting `BasicBlock*` / `Instruction*`.

**Phase 5 (Debug info):**
- `DILocation l(N)` constructor signature changed; needs `DebugLoc` API instead.
- `sys::fs::F_Append` no longer exists; file opening API changed.
- `getPassName()` return type changed from `const char*` to `StringRef`.

**Phase 6 (PostDominanceFrontier):**
- `DominanceFrontierBase` appears not to exist or has changed; `PostDominanceFrontier` won't compile.
- Multiple errors in `PostDominanceFrontier.h` around `iterator`, `DomSetType`, `Frontiers`, etc.
- `PostDominatorTree::ID` no longer exists (no longer a legacy pass with static ID).

These errors begin appearing after ~6 seconds of compilation (after `librtgiri.a` successfully builds)
in files like `lib/Utility/BasicBlockNumbering.cpp`, `lib/Giri/TraceFile.cpp`, etc.

## Phase 0 — Spike: verify `PostDominanceFrontier`'s dependencies exist in LLVM 8

**Why first:** `include/Utility/PostDominanceFrontier.h` is a hand-vendored subclass of LLVM's
`DominanceFrontierBase` (from `llvm/Analysis/DominanceFrontier.h`). `llvm/Analysis/DominatorInternals.h`
is pulled in separately, by `lib/Utility/PostDominatorFrontier.cpp` (note the filename mismatch
with the header — the header is `PostDominanceFrontier.h`/class `PostDominanceFrontier`, but the
`.cpp` is named `PostDominatorFrontier.cpp`; this inconsistency is pre-existing in the codebase, not
a typo introduced by this plan). I don't have confirmed knowledge of whether these still exist,
unchanged, in LLVM 8 — the legacy `DominanceFrontier` analysis was on a deprecation trajectory
somewhere in the 3.x→8 span. This is the one unknown that can materially change the total effort
estimate, so resolve it before scoping the rest.

**Action:**
- Fetch and inspect LLVM `llvmorg-8.0.0` source for `llvm/include/llvm/Analysis/DominanceFrontier.h`
  and `llvm/include/llvm/Analysis/DominatorInternals.h` (see "Obtaining LLVM 8" above for exact URLs
  — this step needs only the raw header text, not a build) for `DominanceFrontierBase`,
  `ForwardDominanceFrontierBase`, and the `calculate()` template this file relies on.
- **If present and API-compatible:** Phase 6 is a small, mechanical include-path fix (see below).
- **If removed or substantially changed (including a 404 on the fetch above):** Phase 6 becomes a
  from-scratch reimplementation of post-dominance-frontier computation (e.g. building it directly
  from `PostDominatorTree`, which is still available). Re-scope Phase 6's estimate accordingly
  before proceeding further.

**Exit criterion:** A written yes/no on API availability, with the LLVM 8 header content (or 404) as
evidence.

## Phase 1 — Replace the build system with CMake

**Why:** `configure.ac` locates LLVM via `ls $llvm_obj/*/bin/llvm-config`, which assumes the old
autoconf/Make out-of-tree build's directory layout (e.g. `Release+Asserts/bin/llvm-config`). LLVM 8
is CMake-only and places `llvm-config` at `<build>/bin/llvm-config` directly — this probe fails
before any C++ code is even reached. LLVM's own autoconf/Make plumbing (which `Makefile.llvm.rules`
vendors a copy of) was deleted from LLVM's tree well before 8.0, so there's nothing upstream to
resync against either. Patching the old detection logic to tolerate a CMake-built LLVM is more work,
and more fragile, than replacing the build system outright.

**Action:**
- Add a top-level `CMakeLists.txt` using `find_package(LLVM 8.0 REQUIRED CONFIG)` /
  `LLVMConfig.cmake`, following the standard out-of-tree-pass pattern (`list(APPEND
  CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})`, `include(AddLLVM)`, `add_llvm_loadable_module` or
  `add_library(... MODULE ...)` for `libdgutility`/`libgiri`, plain `add_executable` for
  `tools/Tracer` and `tools/PrintTrace`, `add_library(... STATIC)` for `runtime/Giri` (`librtgiri`,
  which is C++/pthreads with **no** LLVM dependency — confirmed via its includes — so it doesn't
  even need LLVM's CMake helpers).
- Preserve the existing directory structure (`lib/Giri`, `lib/Utility`, `runtime/Giri`,
  `tools/Tracer`, `tools/PrintTrace`) as CMake subdirectories rather than reorganizing files.
- Retire `configure`, `autoconf/`, `Makefile*` (root, `lib/`, `tools/`, `runtime/`) once the CMake
  build produces equivalent `build/<mode>/lib/*.so` and `build/<mode>/bin/*` outputs — or keep them
  temporarily side-by-side if a staged cutover is preferred, but don't try to make both build systems
  permanently green; pick CMake as the sole build going forward.
- Update `test/UnitTests/Makefile.common` and `test/HelloWorld/Makefile`'s `GIRI_LIB_DIR`/
  `GIRI_BIN_DIR` variables to point at the new CMake build output layout (these are the only things
  in `test/` that need to change in this phase — the `opt -load ... -mergereturn -bbnum -lsnum ...`
  pipeline itself is unaffected).
- Update `utils/build.sh`, `utils/install_llvm.sh`, `Dockerfile`, `.travis.yml` to build LLVM 8 via
  CMake and invoke the new Giri CMake build instead of `configure && make`.

**Exit criterion:** `cmake --build build` produces `libdgutility.so`, `libgiri.so`, `librtgiri.a`,
`tracer`, `prtrace` linked against LLVM 8, even though the C++ inside won't compile yet (that's
Phases 2-6). Getting to "the build system invokes the compiler and fails on LLVM API errors, not on
`llvm-config` not being found" is the actual exit signal here.

**✓ Verified in Docker:** `docker build -t giri-llvm8 .` successfully completes CMake configuration
and invokes the compiler. Build proceeds through `librtgiri.a` and begins compiling the main Giri
passes before hitting the expected Phase 4/5/6 semantic errors (iterator-to-pointer conversion,
`MDNode` API changes, `PostDominanceFrontier` class structure, etc.). This confirms Phase 1's CMake
build system is working correctly and the handoff to Phase 4 is at the right place.

## Phase 2 — Mechanical header/rename fixes

**Why:** A batch of pure renames/moves that happened between 3.4 and 8.0, confirmed by grepping this
codebase's actual includes. Low risk individually; doing them together clears the noise so Phases
3-6 aren't fighting unrelated compile errors.

**Action, file by file (non-exhaustive — anything else the compiler flags belongs here too):**
- `llvm/Analysis/Dominators.h` → `llvm/IR/Dominators.h` + `llvm/Analysis/PostDominators.h`
  (`include/Giri/Giri.h`)
- `llvm/Analysis/Verifier.h` → `llvm/IR/Verifier.h` (`tools/Tracer/Tracer.cpp`)
- `llvm/InstVisitor.h` → `llvm/IR/InstVisitor.h` (`include/Giri/Giri.h`)
- `llvm/Support/InstIterator.h` → `llvm/IR/InstIterator.h` (`lib/Giri/Giri.cpp`)
- `llvm/Support/CFG.h` → `llvm/IR/CFG.h` (wherever used — grep for it)
- `llvm/PassManager.h` → `llvm/IR/LegacyPassManager.h`; `PassManager` → `legacy::PassManager`
  (`tools/Tracer/Tracer.cpp:25,107`)
- `llvm/Support/CallSite.h` → `llvm/IR/CallSite.h` (deprecated-but-present in 8.0; leave as `CallSite`
  for now rather than migrating to `CallBase`, to keep this phase mechanical)
- `OwningPtr<MemoryBuffer> BuffPtr; error_code ec = MemoryBuffer::getFileOrSTDIN(...)`
  (`tools/Tracer/Tracer.cpp:93-94`) → `std::unique_ptr<MemoryBuffer>` +
  `ErrorOr<std::unique_ptr<MemoryBuffer>> BuffOrErr = MemoryBuffer::getFileOrSTDIN(InputFilename)`
  return-based API
- `llvm/Bitcode/ReaderWriter.h` — check it still resolves in 8.0 (it was a compatibility aggregate
  header for a while); split into `llvm/Bitcode/BitcodeReader.h`/`BitcodeWriter.h` if not.

**Exit criterion:** Compile errors from Phase 2's categories are gone; remaining errors are
`DataLayout`-as-pass (Phase 3), metadata (Phase 4), or debug-info (Phase 5) related.

## Phase 3 — Remove `DataLayout`-as-a-pass

**Why:** 3.4 treated `DataLayout` as a required analysis pass:
`AU.addRequired<DataLayout>()` (`include/Giri/Giri.h:59`),
`TD = &getAnalysis<DataLayout>()` (`lib/Giri/TracingNoGiri.cpp:628`),
`Passes.add(new DataLayout(M.get()))` (`tools/Tracer/Tracer.cpp:108`).
This pattern was removed well before 8.0 — `DataLayout` is now a plain member obtained via
`Module::getDataLayout()` / `Function::getParent()->getDataLayout()`, not a pass.

**Action:**
- `include/Giri/Giri.h`: drop `AU.addRequired<DataLayout>()` and the `AU.addPreserved<...>` line if
  present; change `const DataLayout *TD;` usage sites to fetch it on demand from the enclosing
  `Module`/`Function` instead of caching a pass pointer.
- `lib/Giri/TracingNoGiri.cpp:628`: replace `getAnalysis<DataLayout>()` with
  `F.getParent()->getDataLayout()` (adjust to whatever scope `TD` is used in — check all `TD->`
  call sites for correctness, since it changes from a pointer to a value in modern LLVM APIs).
- `tools/Tracer/Tracer.cpp:107-108`: drop the `Passes.add(new DataLayout(M.get()))` line entirely;
  nothing needs to add it as a pass anymore.

**Exit criterion:** No `getAnalysis<DataLayout>()`/`AU.addRequired<DataLayout>()` remain; project
compiles past these sites.

## Phase 4 — Redesign BB/load-store ID encoding (Metadata/Value split)

**Why — this is the one genuine redesign, not a rename:** `lib/Utility/BasicBlockNumbering.cpp:61-67`
and the equivalent in `lib/Utility/LoadStoreNumbering.cpp` encode Giri's internal numbering scheme by
putting a raw `Value*` — specifically a `BasicBlock*` — directly into an `MDNode`'s operand list:

```cpp
Value *ID[2];
ID[0] = BB;
ID[1] = ConstantInt::get(Type::getInt32Ty(Context), id);
return MDNode::getWhenValsUnresolved(Context, ArrayRef<Value*>(ID, 2), false);
```

LLVM 3.6's Metadata/Value split changed `MDNode` operands from `Value*` to `Metadata*`; wrapping a
`Value` now requires explicit `ValueAsMetadata`/`ConstantAsMetadata`, and `getWhenValsUnresolved`
(a forward-reference mechanism tied to the old Value-based uniquing) no longer exists at all.
`BasicBlock` isn't a `Constant`, so there is no direct drop-in replacement for "embed a pointer to
this exact basic block inside a metadata node" the way 3.4 did it — this needs an actual design
decision, not a mechanical substitution.

**Action:**
- Decide the replacement encoding. Recommended: stop trying to store the `BasicBlock*` itself in
  metadata. Instead, attach the numeric ID directly as per-instruction metadata on the block's first
  instruction (`ConstantAsMetadata::get(ConstantInt::get(...))` wrapped in an `MDNode`, via
  `Instruction::setMetadata`), and recover "which BB does this ID belong to" the same way the query
  side already does — by looking up the instruction and asking `getParent()`. This avoids needing a
  Value-shaped metadata operand for the BasicBlock at all.
  - Check `include/Utility/BasicBlockNumbering.h` and `include/Utility/LoadStoreNumbering.h`'s
    `QueryBasicBlockNumbers`/`QueryLoadStoreNumbers` read-side APIs (`getID`, lookup-by-BB,
    lookup-by-ID) before finalizing the encoding — the replacement must support the same queries
    those consumers need (`TraceFile.cpp`, `TracingNoGiri.cpp`, `Giri.cpp` all call `getID(...)`).
- Apply the same redesign to `lib/Utility/LoadStoreNumbering.cpp` (same
  `MDNode::getWhenValsUnresolved` pattern, operating on `Instruction*` instead of `BasicBlock*` —
  this side is actually simpler since instructions can carry their own metadata directly without the
  BB indirection).
- Update the read side (`dyn_cast<MDNode>(MD->getOperand(index))`,
  `NamedMDNode`/`getOrInsertNamedMetadata` usage) to match the new operand types
  (`Metadata*` vs. `Value*`) throughout `BasicBlockNumbering.cpp`, `LoadStoreNumbering.cpp`.

**Exit criterion:** `opt -load libdgutility.so -bbnum -lsnum ... -dump-bbid=true` /
`-dump-lsid=true` (see `test/UnitTests/Makefile.common`'s `bbid`/`lsid` targets) reproduce the same
IDs as before on a small test case (e.g. `test/HelloWorld`), confirming the encoding change is
behavior-preserving before relying on it for slicing.

## Phase 5 — Rewrite debug-info line-mapping calls

**Why:** `lib/Giri/Giri.cpp:352-353` and `lib/Utility/SourceLineMapping.cpp:78-79` do:

```cpp
if (MDNode *N = I->getMetadata("dbg")) {
  DILocation l(N);           // or "Loc"
  ... l.getFilename() ... l.getLineNumber() ... l.getDirectory() ...
}
```

This is 3.4's lightweight `DIDescriptor`-family wrapper over a raw metadata node. By LLVM 8,
`DILocation` is a proper typed `MDNode` subclass reached via `Instruction::getDebugLoc()` (returns a
`DebugLoc`, not something you fish out of generic instruction metadata by string key), and
filename/directory require walking the `DIScope`/`DIFile` chain rather than reading flat fields.

**Action:**
- `lib/Giri/Giri.cpp`: replace `I->getMetadata("dbg")` + `DILocation l(N)` with
  `if (const DebugLoc &DL = I->getDebugLoc()) { ... DL->getFilename() ... DL.getLine() ... }` (or
  `DL->getScope()->getFilename()` depending on exactly what 8.0's `DILocation` exposes directly —
  confirm against `llvm/include/llvm/IR/DebugInfoMetadata.h` in the vendored LLVM 8 source).
- `lib/Utility/SourceLineMapping.cpp:78-88`: same rewrite; note this call site also reads
  `Loc.getDirectory()`, which in modern LLVM comes off the `DIFile`/`DIScope`, not the location
  itself — check both are actually still populated the same way for `clang -g -O0` output before
  assuming parity.
- These are the only two call sites; no broader debug-info handling exists elsewhere in the codebase
  (confirmed via grep for `DebugInfo`/`DILocation`/`DIScope`/`MDNode.*dbg`).

**Exit criterion:** `test/UnitTests/test10` (or any test with a `criterion-loc`-based test, e.g.
`matrix_multiply`) resolves the same source `file:line` as before, confirmed by diffing
`*.slice.loc` against the checked-in `ans-loc-*.txt`.

## Phase 6 — Fix `PostDominanceFrontier` (scope depends on Phase 0 outcome)

**If Phase 0 found `DominanceFrontierBase`/`DominatorInternals.h` still present and compatible:**
- Update `include/Utility/PostDominanceFrontier.h`'s includes to their LLVM 8 locations (likely still
  `llvm/Analysis/DominanceFrontier.h`, `llvm/Analysis/PostDominators.h` — confirm exact paths from
  the Phase 0 spike) and fix any signature drift in `DominanceFrontierBase`'s constructor/`calculate`
  member (LLVM's dominator-tree-adjacent APIs churned their exact call signatures multiple times in
  this span even where the overall design survived). Also update
  `lib/Utility/PostDominatorFrontier.cpp`'s `llvm/Analysis/DominatorInternals.h` include the same way.

**If Phase 0 found them removed/incompatible:**
- Reimplement post-dominance-frontier computation directly against `PostDominatorTree` (still
  available and stable in LLVM 8), using the standard dominance-frontier algorithm (for each node,
  frontier members are successors not strictly post-dominated by the node, propagated up the
  post-dominator tree) rather than relying on LLVM's now-removed generic template. This is more work
  but self-contained — `PostDominanceFrontier`'s public surface (`getAnalysisUsage`, whatever
  `DynamicGiri::findExecForcers`/`getBackwardsSlice` in `lib/Giri/Giri.cpp` actually call on it) can
  stay the same, only the internals change.

**Exit criterion:** `DynamicGiri`'s control-dependence logic (`findExecForcers`, which consumes
`PostDominanceFrontier`) produces the same slice output as pre-port on all control-flow-heavy tests
(`UnitTests/test8`, `test9`, and anything with branches/loops — check `auto-tests.txt` for which
those are).

## Cross-cutting: verification after each risky phase

Run, after Phases 3 through 6 individually (not batched):

```bash
cmake --build build
make -C test test          # full suite against auto-tests.txt
```

Any `ans-*.txt`/`ans-loc-*.txt`/`ans-inst-*.txt` diff after a phase should be root-caused to that
phase's change before proceeding — don't let failures accumulate across phases, since Phase 4 and 5
both touch instruction/BB identification and a bug in one can masquerade as a bug in the other if
they're not verified independently.

## Risk register

| Item | Confidence | Why |
|---|---|---|
| Phase 1 (CMake) | High | Standard, widely-documented pattern; `librtgiri` has zero LLVM dependency, further reducing risk |
| Phase 2 (renames) | High | Confirmed via direct grep of this codebase's includes against known LLVM header-move history |
| Phase 3 (`DataLayout`) | High | Small, fully-enumerated call-site list (3 files) |
| Phase 4 (metadata redesign) | Medium | Real design decision required; correctness depends on matching existing read-side query semantics exactly |
| Phase 5 (debug info) | Medium | Only 2 call sites, but `getDirectory()` semantics need explicit verification against LLVM 8's `DIScope`/`DIFile` |
| Phase 6 (`PostDominanceFrontier`) | **Unresolved — Phase 0 gates this** | Haven't confirmed `DominanceFrontierBase`/`DominatorInternals.h` availability in real LLVM 8 headers yet |

## Rough sequencing / effort shape

Phase 0 (spike) → Phase 1 (build) → Phase 2 (mechanical) can proceed with reasonable confidence
back-to-back. Phases 3-6 should each get their own verification pass against the test suite before
starting the next, per the cross-cutting note above. Phase 6's effort is bimodal (small fixup vs.
from-scratch reimplementation) and won't be known precisely until Phase 0 completes — resist the urge
to estimate total project time until that spike is done.
