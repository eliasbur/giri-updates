---
title: Close the LLVM 5.0.2 port's verification and documentation debt.
status: open
priority: medium
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-12
---

## Goal

Discharge the parts of the port that were declared done without evidence: verify the three critical
invariants, and bring the notes and reports in line with what is actually true on the branch.

## Why this task exists

`llvm-5-port.md` is marked `status: done`, but four of its Definition-of-done boxes are still
unchecked, and one of them is substantive rather than clerical:

> - [ ] The three invariants in `AGENTS.md` verified or their deviation explained in the PR

It has no evidence anywhere in `porting/`. That matters most for **invariant 1**: the port
rewrote both numbering passes (`lib/Utility/BasicBlockNumbering.cpp` and
`lib/Utility/LoadStoreNumbering.cpp`, ~220 lines changed against `master`) precisely because 3.4's
approach of stashing `Value*` inside `MDNode` no longer exists — and numbering determinism across
the instrumentation run and the slicing run is what makes a trace interpretable at all. 20 passing
tests are indirect evidence, not a check.

## Part 1 — verify the three invariants (`AGENTS.md` → "Critical invariants")

1. **Numbering determinism.** `-bbnum` / `-lsnum` must assign identical IDs in the instrumentation
   pipeline and the slicing pipeline. Both pipelines run over the same `.all.bc` but with different
   surrounding passes (`test/Makefile.common:36-59`). Dump the IDs from each configuration
   (`opt … -bbnum -dump-bbid=true` and `opt … -lsnum -dump-lsid=true`, with and without
   `-trace-giri` / `-dgiri` in the pipeline) and diff them. Do this for at least one single-file
   unit test and one multi-file test (`llvm-link` of several `.bc` files is where iteration order
   is most likely to differ). Also confirm two consecutive runs of the same command agree — the
   rewritten passes must not depend on pointer-valued container ordering.
2. **`Entry` struct ABI.** `include/Giri/Runtime.h` is byte-identical to `master` on this branch
   (`git diff master..HEAD -- include/Giri/Runtime.h` is empty) — record that as the layout half of
   the check. The remaining half is the size-divides-page-size property (`Runtime.h:68-71`): confirm
   `sizeof(Entry)` on the 5.0.2 build divides the page size, with the number written down.
3. **Debug info.** `-g` must still yield correct `file:line` mapping through
   `lib/Utility/SourceLineMapping.cpp` (rewritten for LLVM 5's metadata classes, ~108 lines). Run
   the `mapping` target (`test/Makefile.common:68`) on one test and show that the mapping is
   populated and matches the source, rather than inferring it from the slice diffs.

Write the outcome — verified, or deviating and why — into this note and into `AGENTS.md`'s
`## Current state`.

> **Deliberately out of scope: the `api-breakings.yaml` triage.**
> `porting/llvm-releases/5.0.0/api-breakings.yaml` has 388 entries, of which exactly 4 carry
> `relevance: "affected"` / `status: "addressed"`; the other 384 are still `relevance: "unknown"` /
> `status: "pending"`, including changes the port demonstrably acted on (header moves,
> `getOrInsertFunction`, the debug-info metadata rewrite, `OwningPtr`, `DataLayout`, the
> `DominanceFrontier` family). Finishing that sweep was deferred by decision on 2026-08-12 — do not
> pick it up here, and leave `llvm-5-port.md`'s corresponding checkbox unticked with a pointer to
> this paragraph.

## Part 2 — reconcile the notes and reports

- `llvm-5-port.md`: tick or strike the four stale Definition-of-done boxes; the `AGENTS.md`
  `## Current state` box is now satisfied.
- `llvm-5-test-fixes.md`: two boxes are stale — the PR (`giriupdates #7`) is merged as `3b26ea6`,
  and the suite line ("21 of 22 pass; kmeans is the only expected failure") does not match the
  recorded result (the one failure is `matrix_multiply-seq`; kmeans-seq's PASS survived the honest
  harness, kmeans-pthread was never re-run — see `llvm-5-harness-fallout` and `llvm-5-kmeans`).
- **Reconcile the four suite results that now exist**, in one place, and say which one is current:
  13 PASS / 9 FAIL (baseline, pre-`3b26ea6`), 21 PASS / 1 FAIL (post-fixes, suppressive harness),
  7 PASS / 15 FAIL (`2fb3b6d`, honest harness before the exit-status opt-in), and 21 PASS / 1 FAIL
  (`5fbca9d`, honest harness with `EXPECTED_EXIT` — the per-test table in
  `llvm-5-harness-fallout.md` is the authoritative version). A reader currently has to know the
  commit history to tell which number applies to the tree in front of them.
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md`'s per-test verdict table is **pre-`3b26ea6` and
  pre-honest-harness**, and every `FAIL-BUG` row attributed to "Root cause A" was subsequently
  fixed. Do not rewrite the verdicts — downstream tasks treat them as the audit's record — but head
  the table with the commit it describes, so it is not read as the current state.
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md`: the "Root cause A — 8 tests" heading sits above a
  10-test list, and the reconciliation block below the verdict table is unreconciled scratch work
  ("**Wait** — re-checking the baseline: … let me recount"). Rewrite that section to state the
  final numbers once. Leave the per-test verdict table alone — downstream tasks treat it as
  authoritative — and coordinate with `llvm-5-seq-variant-failures`, which also edits this file.
- Record a decision on `test/UnitTests/{test6,test7,test22}`: each has a golden file but is absent
  from `test/auto-tests.txt`. Either wire them in or write the exclusion reason where the suite
  list lives, so it is not rediscovered by the next audit.

## Part 3 — housekeeping

- `test/matrix_multiply/` still holds build artifacts from the last fix session
  (`matrix_multiply-pthread.{all.bc,bc,trace,trace.bc,trace.exe,trace.s,slice,slice.loc}` and
  `matrix_file_out_pthreads.txt`). They are gitignored, but `make clean -C test/matrix_multiply`
  and `clean-all` should be run so the tree matches a fresh checkout.
- `porting/AgentGuide.md` mentions `DEBUGFLAGS` and the pipeline but not that `-debug` /
  `-debug-only=` are inert on a no-asserts toolchain and that Giri's own `assert()`s are compiled
  out by the `Release` CMake build. Both cost previous tasks time; add a line.

## Definition of done

- [ ] Invariant 1 verified with dumped ID sets from both pipelines, for at least one single-file and
      one multi-file test, plus a repeat-run comparison; the diffs (empty or not) recorded
- [ ] Invariant 2 verified: `Runtime.h` unchanged against `master`, and `sizeof(Entry)` on the
      5.0.2 build recorded together with the page size it divides
- [ ] Invariant 3 verified from a `-srcline-mapping` run, not inferred from test diffs
- [ ] `llvm-5-port.md` and `llvm-5-test-fixes.md` checkboxes match reality, with the deferred
      `api-breakings.yaml` box left unticked and annotated
- [ ] `SUMMARY.md`'s root-cause counts and reconciliation section state one consistent set of
      numbers; the per-test verdict table is untouched apart from a heading naming the commit it
      describes
- [ ] The four historical suite results reconciled in one place, with the current one identified
- [ ] Decision recorded for `test6` / `test7` / `test22`
- [ ] `test/matrix_multiply` cleaned; `porting/AgentGuide.md` gained the no-asserts note
- [ ] `AGENTS.md`'s `## Current state` updated to reflect the invariant results
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## Traps

- Everything that runs `opt` runs inside the Giri container, never in the devcontainer
  (`AGENTS.md` → "Containers — two kinds").
- The `bbid` / `lsid` targets in `test/Makefile.common` pipe into `view -` and hang under
  `docker exec` — run the underlying `opt … -dump-bbid=true` / `-dump-lsid=true` directly.
- This task edits `SUMMARY.md`, and so does `llvm-5-seq-variant-failures`. Rebase rather than
  resolving by hand-merging two rewrites of the same section.
- Do not change any `ans-*.txt`, criterion file, or the test Makefiles; the remaining harness work
  belongs to `llvm-5-harness-signal-detection`. Cleaning the tree is the only test-adjacent action
  in scope here.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `porting/TaskNotes/Tasks/llvm-5-port.md`, `porting/TaskNotes/Tasks/llvm-5-test-fixes.md`
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md`
- `porting/AgentGuide.md`, `AGENTS.md`, `test/auto-tests.txt` (only if test6/7/22 are wired in)
- `porting/TaskNotes/Tasks/llvm-5-port-closeout.md` (this note — progress log)
- Read-only: `lib/**`, `include/**`, `runtime/**`, `tools/**`, `test/**` apart from cleaning

## Blocked by

- ~~llvm-5-test-fixes~~
- ~~llvm-5-harness-honesty~~
- ~~llvm-5-harness-fallout~~
- ~~llvm-5-harness-residuals~~
- llvm-5-seq-variant-failures
- llvm-5-kmeans
- llvm-5-harness-signal-detection

This task runs last: the invariant checks can be done at any time, but the note and report
reconciliation is only final once the others have stopped moving the numbers.

## Progress log

## Handoff
- branch `agent/llvm-5-port-closeout`
Refs: `porting/TaskNotes/Tasks/llvm-5-port.md`, `porting/TestAudit/llvm-5.0.2/SUMMARY.md`,
`porting/llvm-releases/5.0.0/api-breakings.yaml`, `AGENTS.md`, `porting/HowItWorks.md`
