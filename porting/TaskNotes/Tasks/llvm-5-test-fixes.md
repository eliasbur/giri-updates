---
title: Fix confirmed test failures introduced by the LLVM 5.0.2 port.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-11
---

## Goal

Fix two confirmed defects in the LLVM 5.0.2 port that cause 11 test failures (out of 22).

## Why this task exists

The test audit (`porting/TestAudit/llvm-5.0.2/SUMMARY.md`) found:
1. A port-introduced bug in `PostDominatorFrontier.cpp:37` that breaks control-dep computation for 10 tests.
2. A latent namespace collision in `TraceFile.cpp` that crashes on test16.

## Definition of done

- [ ] Fix `PostDominatorFrontier.cpp:37`: restore the 3.4 structure where child recursion runs even for null-BB (virtual root) nodes
- [ ] Fix `TraceFile.cpp:378`: offset or separate BB IDs and instruction IDs to prevent `findNextNestedID` namespace collision
- [ ] Verify `properlyDominates` overload agrees with 3.4 behavior after the fix
- [ ] Full suite: at least 20 of 22 tests pass (kmeans remains FAIL-HARNESS pending harness fix)
- [ ] No golden files, test Makefiles, or build files modified
- [ ] PR opened into `port/llvm-5.0.2` and linked below

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
**Original 3.4 code:**
```cpp
DomSetType &S = Frontiers[BB];
if (getRoots().empty()) return S;
if (BB)
    for (pred_iterator SI = pred_begin(BB), SE = pred_end(BB);
         SI != SE; ++SI) {
```
**Intended fix:** Replace `if (!BB) return S;` with an `if (BB) { }` guard that wraps only the predecessor loop (lines 39-44), allowing the child recursion loop (lines 46-55) to execute for virtual-root nodes. Also update `properlyDominates` at line 52 to handle null `Node->getBlock()`.

**Affected tests:** test3, test5, test8, test9, test10, test11, test12, test17, matrix_multiply, pca

## Defect 2: TraceFile.cpp:378 namespace collision (FAIL-BUG, 1 test)

**File:line:** `lib/Giri/TraceFile.cpp:378-410`
**Current behavior:** `findNextNestedID` uses an instruction-level ID as `nestID` but searches for BB entries. When a BB ID matches the instruction ID, nesting is falsely incremented and the search fails with `report_fatal_error`.
**Intended fix:** Offset instruction IDs by a constant (e.g., `MAX_BB_COUNT + 1`) or use a distinct tag bit so BB IDs and instruction IDs never collide. Alternatively, refactor `findNextNestedID` to not conflate the two ID namespaces.

**Affected tests:** test16

## Files / scope

- `lib/Utility/PostDominatorFrontier.cpp` (fix defect 1)
- `lib/Giri/TraceFile.cpp` (fix defect 2)
- Read-only: `test/**/ans-*.txt`, `test/Makefile*`, `CMakeLists.txt`, `Dockerfile`, `utils/**`

## Blocked by

- ~~llvm-5-test-audit~~

## Progress log

## Handoff
- branch: (driver-resolved)
Refs: `porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/TaskNotes/Tasks/llvm-5-test-audit.md`