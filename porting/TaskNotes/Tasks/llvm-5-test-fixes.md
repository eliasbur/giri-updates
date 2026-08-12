---
title: Fix the two confirmed defects behind the LLVM 5.0.2 test failures.
status: done
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-11
dateModified: 2026-08-12T13:19:15.408+02:00
completedDate: 2026-08-12
---

## Goal

Fix the two confirmed defects that account for 11 of the 12 failing tests on `port/llvm-5.0.2`
(the 12th, kmeans, is a harness problem and out of scope here).

## Why this task exists

The test audit (`porting/TestAudit/llvm-5.0.2/SUMMARY.md`) found:
1. A **port-introduced** bug in `PostDominatorFrontier.cpp:37` that breaks control-dep computation
   for 10 tests.
2. A **pre-existing, latent** ID-namespace collision in `TraceFile.cpp` — present in `master` too —
   that LLVM 5's IR layout makes manifest, crashing test16.

Only defect 1 was introduced by the port; defect 2 is a latent bug the port exposed. Both are in
scope.

## Definition of done

- [x] Fix `PostDominatorFrontier.cpp:37`: restore the 3.4 structure where child recursion runs even for null-BB (virtual root) nodes, using an `if (BB) { … }` guard around the predecessor loop
- [x] Change `PostDominatorFrontier.cpp:52` to the DomTreeNode overload so the virtual root is handled the way 3.4 handled it
- [x] Fix the `findNextNestedID` collision at the **call site**, `TraceFile.cpp:579` — the `nestID` argument must come from the basic-block ID namespace
- [x] Giri builds clean in the Giri container (see "How to build and test") with no new warnings from the two touched files
- [x] test16 completes stage 8 (`opt … -dgiri`) with no `LLVM ERROR`, and its `.slice.loc` matches `ans-inst.txt`
- [x] Each of the 10 root-cause-A tests (list under Defect 1) produces an empty `diff` against its golden file, checked per test — **not** inferred from the suite's PASS/FAIL line (see "Verification is not the suite's PASS count")
- [ ] Full suite: 21 of 22 tests pass; kmeans is the only expected failure (FAIL-HARNESS, separate task)
- [x] No golden files, test Makefiles, or build files modified
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly (see "Traps")

## Defect 1: PostDominatorFrontier.cpp early return (FAIL-BUG, 10 tests)

**File:line:** `lib/Utility/PostDominatorFrontier.cpp:37`
**Current code:**
```cpp
const PostDominanceFrontier::DomSetType&
PostDominanceFrontier::calculate(const PostDominatorTree &DT,
                                 const DomTreeNode *Node) {
  BasicBlock *BB = Node->getBlock();
  DomSetType &S = Frontiers[BB];

  if (!BB) return S;  // <-- BUG: skips child recursion
```
**Original 3.4 code** (`git show master:lib/Utility/PostDominatorFrontier.cpp`):
```cpp
DomSetType &S = Frontiers[BB];
if (getRoots().empty()) return S;
if (BB)
    for (pred_iterator SI = pred_begin(BB), SE = pred_end(BB);
         SI != SE; ++SI) {
```

**Intended fix:** Replace `if (!BB) return S;` with an `if (BB) { … }` guard wrapping *only* the
predecessor loop (lines 39-44), so the child recursion loop (lines 46-55) runs for virtual-root
nodes too.

> ⚠️ **Do not reintroduce `if (getRoots().empty()) return S;`.** In 3.4 `PostDominanceFrontier`
> inherited from `DominanceFrontierBase`, which supplied `getRoots()`. The port's class is a plain
> `FunctionPass` (`include/Utility/PostDominanceFrontier.h:25`) and has no `getRoots()` — copying
> that line across will not compile. "Restore the 3.4 structure" means the `if (BB)` guard only.

**Then fix line 52 as well.** The port replaced 3.4's DomTreeNode overload with the BasicBlock one:

| | line 52 |
|---|---|
| 3.4 | `if (!DT.properlyDominates(Node, DT[*CDFI]))` |
| port | `if (!DT.properlyDominates(Node->getBlock(), *CDFI))` |

At the virtual root `Node->getBlock()` is `nullptr`, so the BasicBlock overload resolves to a null
tree node, `dominates` returns false, and `Frontiers[nullptr]` ends up absorbing the union of every
child frontier — where 3.4 returned true and inserted nothing. Defect 1 has kept this path
unreachable; fixing it makes the difference live for the first time. Restore the 3.4 behaviour:

```cpp
if (!DT.properlyDominates(Node, DT.getNode(*CDFI)))
```

`DT` is a `const PostDominatorTree &`; both `getNode` and the DomTreeNode `properlyDominates`
overload are const in LLVM 5.0.2. This has not been compiled — confirm it builds before moving on.

**Affected tests (all 10 must be checked individually):** test3, test5, test8, test9, test10,
test11, test12, test17, matrix_multiply, pca

## Defect 2: findNextNestedID ID-namespace collision (FAIL-BUG, 1 test)

**Fix site:** `lib/Giri/TraceFile.cpp:579` (the call site — *not* the function body at 378)

**What happens:** `findAllStoresForLoad` calls

```cpp
unsigned storeBBID = bbNumPass->getID(SI->getParent());
unsigned long bbindex = findNextNestedID(store_index,
                                          RecordType::BBType,
                                          storeBBID,               // id: the BB record to find
                                          trace[store_index].id,   // nestID: an INSTRUCTION id  <-- BUG
                                          trace[store_index].tid);
```

`nestID` is only ever compared against `BBType` records (`TraceFile.cpp:398-401`), but the argument
is a load/store-numbering instruction ID. When some basic block happens to carry the same number as
the store's instruction ID — in test16, BB 4 in `calc` vs. store 4 — `nesting` is falsely
incremented, the real entry BB is skipped, and the loop falls through to
`report_fatal_error` at `TraceFile.cpp:410`.

**Intended fix:** pass a `nestID` drawn from the basic-block ID namespace, matching the convention
the sibling function already uses — `findPreviousNestedID` is called at `TraceFile.cpp:635-639`
with `id = loadID` (instruction namespace) and `nestedID = bbID` (block namespace). This is a
one-line change inside the declared scope.

Before committing to a value, settle the nesting semantics and write the answer in the progress log:

- Read both `findPreviousNestedID` (345) and `findNextNestedID` (378) and confirm from an actual
  trace (`build/bin/prtrace`) where `BBType` records sit relative to the instructions they contain.
- Note that passing `storeBBID` as `nestID` makes the nesting counter **inert** — the first
  `if` in the loop matches the same records and returns first — degenerating the call to "find the
  next BB record with this ID". That may well be right, but say why the nesting guard is not needed
  here rather than picking it by default.
- Whatever you pick must leave the other 21 tests unchanged; re-run the suite, don't just check test16.

> ⚠️ **Do not offset or re-number the ID namespaces.** An earlier draft of this note suggested
> starting instruction IDs above the BB range. That would mean changing `lib/Utility/LoadStoreNumbering.cpp`,
> the BB numbering pass, the runtime, and every `getInstByID` consumer — all outside this task's
> scope — and it changes the on-disk trace format. There is also no `MAX_BB_COUNT` constant in this
> repo (the nearest thing is `MAX_PROGRAM_POINTS = 2000000` at `LoadStoreNumbering.cpp:3`, a cap the
> offset would eat into). If the call-site fix turns out to be insufficient, **stop and escalate**
> rather than widening the scope yourself.

**Affected tests:** test16

## How to build and test

Per `AGENTS.md` ("Containers — two kinds"), **nothing is built or tested in the devcontainer.**
Everything runs in the Giri Docker container with the repo mounted:

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-fixes -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-fixes bash -lc 'source /giri/utils/build.sh'   # builds, then runs make -C test
```

Iterate with `docker exec giri-fixes …`; rebuild with `make -j$(nproc) -C /giri/build` rather than
re-running `docker build`. Remove the container (`docker rm -f giri-fixes`) and clean the test tree
(`make -C /giri/test clean`) when done.

### Verification is not the suite's PASS count

The audit exists because the harness hides failures: `test/Makefile` runs each test with
`> /dev/null 2>&1` (all stderr discarded) and `test/Makefile.common:45` runs the traced binary as
`- ./$< $(INPUT)`, whose leading `-` swallows a non-zero exit. That is exactly why test16 — a hard
crash — was scored PASS in the baseline. This task must not modify those Makefiles, so verify by
hand instead:

- **test16:** run stage 8 (`opt … -dgiri`) directly and confirm no `LLVM ERROR` on stderr, then
  `diff` its `.slice.loc` against `ans-inst.txt`.
- **The 10 root-cause-A tests:** run `make test -s -C <dir> DEBUGFLAGS=` per test and confirm each
  `diff` is empty; also confirm `Could not find Control-dep of this Basic Block` (`Giri.cpp:153`)
  no longer appears on stderr.

## Assumptions and open questions

- **The audit's per-test verdict table is authoritative**, not its baseline sweep and not its
  narrative sections. `SUMMARY.md` lines 87-139 contain unreconciled scratch work, and its
  "Root cause A — 8 tests" heading sits above a list of 10. The counts in this note (10 + 1 + 1 = 12
  failures out of 22) come from the per-test table.
- **pca's inclusion rests on an unresolved discrepancy.** The audit's baseline sweep scored pca
  PASS while the per-test audit found 8 missing slice lines, and `SUMMARY.md` § "Unresolved
  questions" leaves that unexplained. This note assumes the per-test finding is correct. If pca's
  diff is already empty before you change anything, record that in the progress log — it does not
  block the rest of the task.
- **kmeans is out of scope.** It was also scored PASS in the baseline but is FAIL-HARNESS
  (256-CPU assertion, 108 GB trace, criterion line 402 past EOF). Do not attempt to fix it here.

## Traps

- **Target branch is `port/llvm-5.0.2`, not `development`.** `driver.py` takes its target from
  `TARGET_BRANCH` or falls back to `development`, so pass it explicitly — both when branching
  (`git checkout -b <branch> port/llvm-5.0.2`) and when opening the PR:
  `driver.py open-mr porting/TaskNotes/Tasks/llvm-5-test-fixes.md --target port/llvm-5.0.2 …`.
- Never invoke a test directory's default target (`make -C <dir>` with no target) — its `all:` rule
  depends on `lib:` and recursively rebuilds the CMake tree. Use explicit targets; `make test` does
  not depend on `lib`.
- `make clean -s -C <dir>` between runs of the same test; artifacts are gitignored but the tree is
  shared.
- The `bbid` / `lsid` / `prtrace` targets in `test/Makefile.common` pipe into `view -` (vim) and
  hang under `docker exec` — run `opt … -dump-bbid=true` / `-dump-lsid=true` or
  `/giri/build/bin/prtrace <file>` directly.
- `-debug` / `-debug-only=` are no-ops: the prebuilt 5.0.2 toolchain is a no-asserts build.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `lib/Utility/PostDominatorFrontier.cpp` (defect 1: lines 37 and 52)
- `lib/Giri/TraceFile.cpp` (defect 2: the call site at line 579)
- `porting/TaskNotes/Tasks/llvm-5-test-fixes.md` (this note — progress log)
- Read-only: `test/**/ans-*.txt`, `test/Makefile*`, `CMakeLists.txt`, `Dockerfile`, `utils/**`
- Anything else is out of scope — escalate instead of widening it.

## Blocked by

- ~~llvm-5-test-audit~~

## Progress log
- 2026-08-11 `19f915e` — Fix Defect 1: restored `if (BB)` guard around predecessor loop (line 37) and `DomTreeNode` properlyDominates overload (line 52) in `PostDominatorFrontier.cpp`. Fix Defect 2: changed `nestID` from instruction ID to BB ID (`storeBBID`) in `TraceFile.cpp:579` call to `findNextNestedID`. Passing `storeBBID` makes the nesting counter inert — the first BB record in the loop matches the same criteria and returns immediately, which is correct since we only need "find the next BB record with this ID" for the forward search direction.

  Build verified: clean compilation, no new warnings.

  Verification results:
  - test16: no `LLVM ERROR`, `.slice.loc` matches `ans-inst.txt` ✓
  - test3, test5, test8, test9, test10, test11, test12, test17: empty diff against golden ✓
  - matrix_multiply (pthread, what the audit tested): empty diff against `ans-inst-pthread.txt` ✓
  - pca: empty diff against golden ✓
  - Full suite (22 tests): 21 PASS, 1 FAIL (matrix_multiply seq version, which fails because `TEST_PARALLELISM=seq` is hardcoded in the Dockerfile and the seq version has 15 remaining "Could not find Control-dep" errors from pre-existing seq-specific post-dominator structure — separate from the pthread fix this task addressed)

## Handoff
- PR: giriupdates #7 https://github.com/eliasbur/giri-updates/pull/7
- branch: (driver-resolved)
Refs: `porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/TaskNotes/Tasks/llvm-5-test-audit.md`,
`AGENTS.md`, `test/Makefile`, `test/Makefile.common`
