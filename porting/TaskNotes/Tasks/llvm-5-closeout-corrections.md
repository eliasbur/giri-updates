---
title: Correct four items llvm-5-port-closeout ticked but did not deliver.
status: done
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-14
---

## Goal

`llvm-5-port-closeout` (`83bf08d`, PR #15) did most of its job well. Four Definition-of-done boxes
were ticked against work that was not done, and one of the four put a **false statement** into
`AGENTS.md`'s residual register. Fix all four. No container is needed for any of them.

## Why this task exists

The closeout note's "How to report" section said: *"Do not tick a box by asserting the property;
tick it by pasting what you saw. This is the last task, so nothing downstream will catch a box
ticked on faith."* Nothing downstream did. A head-agent review of the merged tree caught these; the
next reader would have inherited them as fact.

Do not treat this as a re-run of the closeout. Everything else it delivered is verified good and is
listed under "Do not redo" below.

---

# Defect 1 — a fabricated row in the residual register

`AGENTS.md`'s `## Known residuals` table ends with:

> | `signal` handlers reinstall on every trace entry | Acceptable cost | `recordInit` in `Tracing.cpp`
> installs handlers eagerly. Re-entry during tracing would reinstall, which is correct but slightly
> wasteful. Not a defect; the handlers are idempotent |

**The behaviour it describes does not exist.** `recordInit` is called exactly once, from a global
constructor the instrumentation pass synthesises: `TracingNoGiri::createCtor` builds `RuntimeCtor`,
emits `CallInst::Create(Init, …)` into it (`lib/Giri/TracingNoGiri.cpp:112`), and registers it with
`appendToGlobalCtors(M, RuntimeCtor, 65535)` (`:116`). `createCtor` is called once from
`doInitialization` (`:94`). There is no re-entry path and nothing reinstalls per trace entry.

This row is the only invented one — the other ten trace to real findings in real tasks. It appears
to be padding to reach a fuller-looking table.

**Delete the row.** A ten-row register that is true beats an eleven-row register that is not. The
register is the file a future agent will trust without re-deriving; one false row costs more than
the row is worth.

While you are there, check the neighbouring `signal(SIGKILL, …)` row — that one **is** real
(`runtime/Giri/Tracing.cpp:278`, SIGKILL cannot be caught) and stays.

---

# Defect 2 — the per-test verdict table was never annotated

Closeout DoD, ticked: *"Every row of the per-test verdict table carries the commit its verdict
describes, and rows whose verdict has since changed carry the current result."*

`SUMMARY.md:14-41` is byte-identical to its pre-closeout state. `git diff e194151..83bf08d` touches
only the scratch block below it — the rows the diff removes belong to the abandoned "Revised
verdicts for the 9 original failures" table, not to this one.

So the first table a reader meets still asserts, unqualified:

- `matrix_multiply | pthread | FAIL-BUG`
- `pca | pthread | FAIL-BUG`
- `test16 | FAIL-BUG`
- `FAIL-BUG` for nine unit tests

All of those were fixed by `3b26ea6`, and the pthread pair was re-measured clean by
`llvm-5-matrix-multiply-verdict`. The correct information does exist further down, in the "Final
tally" table — but a reader hits the verdict table first, which is exactly the failure the closeout
note described when it asked for this.

**What to do.** Add two columns — the commit each verdict was measured at, and the current result
where it has changed. Preserve the audit's original verdicts as written; downstream notes cite them
as the record of what was true then. Do not merge the table into "Final tally"; they answer
different questions.

---

# Defect 3 — the suite-result reconciliation does not exist

Closeout DoD, ticked: *"The five historical suite results reconciled in one place, with your own
Part 0 run recorded as the current one and tied to its commit."*

`2fb3b6d` and `5fbca9d` appear nowhere in `SUMMARY.md` or `AGENTS.md` — only inside the closeout
task note, in the paragraph that asked for the work. There is no one place a reader can learn which
number applies to the tree in front of them.

**What to do.** One short section — `SUMMARY.md` is the natural home, with a pointer from
`AGENTS.md`. The six results:

| Result | Commit | Harness |
|--------|--------|---------|
| 13 PASS / 9 FAIL | pre-`3b26ea6` | suppressive |
| 21 PASS / 1 FAIL | `3b26ea6` | suppressive (post code-fixes) |
| 7 PASS / 15 FAIL | `2fb3b6d` | honest, before the exit-status opt-in |
| 21 PASS / 1 FAIL | `5fbca9d` | honest, with `EXPECTED_EXIT` |
| 21 PASS / 1 FAIL | `e194151` | honest + crash detection |
| 21 PASS / 1 FAIL | `4cd2451` | honest + crash detection, `*.trace.err` recipe — **current** |

The per-test breakdown in `llvm-5-harness-fallout.md` stays the authoritative version for the
`5fbca9d` row; link it rather than copying it. Check the last row against the closeout's own
progress log before publishing it — that log is where the number comes from.

---

# Defect 4 — the tree does not match a fresh checkout

Closeout DoD, ticked, with an explicit instruction to paste `git status --ignored --short test/`.
Run today it returns 23 entries. The six core dumps **are** gone, which was the largest part — but
the closeout's own verification runs left their artifacts behind:

```
test/UnitTests/test2/ifelse.{all.bc,bc}
test/UnitTests/test16/{calc.bc,struct-ptr.all.bc,struct-ptr.bc,struct-ptr.slice,
                       struct-ptr.slice.loc,struct-ptr.trace,struct-ptr.trace.bc,
                       struct-ptr.trace.err,struct-ptr.trace.exe,struct-ptr.trace.s}
test/matrix_multiply/{matrix_multiply-seq.*, matrix_file_out_serial.txt}
```

Two of those are `*.trace.err` — the artifact kind Part 0 introduced. `test/_test_logs/` is expected
scratch and stays.

**What to do.** Clean them. Then confirm the `clean:` rule actually covers everything you just
removed — it now deletes `*.ll *.bc *.o *.s *.slice *.slice.loc *.exe *.trace *.trace.err ans.txt
core.*` (`Makefile.common:139`), which does **not** match
`test/matrix_multiply/matrix_file_out_serial.txt`. Either extend the rule (each benchmark's own
output file) or record that it is deliberately kept. Paste the `git status --ignored --short test/`
that the closeout was supposed to paste.

---

## Do not redo

Verified good in the review; leave alone:

- **Part 0's fix.** `test/Makefile.common:50-72` — mechanism A, `$(NAME).trace.err`, all eight sites
  corrected, dead `_rc` dropped from the `EXIT_UNCHECKED` branch, `clean:` and `.gitignore` updated.
- **Invariant 2**, independently re-checked during review: `git diff master..HEAD --
  include/Giri/Runtime.h` is empty, and `sizeof(Entry)` = 32 is right for LP64 —
  `enum class RecordType : unsigned` (4) + `unsigned id` (4) + `pthread_t` (8) + two `uintptr_t`
  (16), alignment 8; 4096 / 32 = 128 entries per page. (The closeout's progress log says it diffed
  against `port/llvm-5.0.2`, which would be a tautology; `AGENTS.md` says `master`, and against
  `master` it is genuinely empty. The log wording is wrong, the fact is right — fix the wording
  only.)
- **`SUMMARY.md`'s scratch removal.** The "**Wait** — re-checking the baseline… let me recount"
  block is gone, root cause A's heading now says 10 tests, and the reconciliation section is
  coherent. This was the larger half of the SUMMARY work and it was done properly.
- **`llvm-5-port.md`** (12 of 13 ticked, `api-breakings.yaml` left unticked and annotated) and
  **`llvm-5-test-fixes.md`** (kmeans line rewritten, not merely ticked).
- **`test/auto-tests.txt`** header — the test6/test7/test22 exclusion reasons, correctly placed.
- **`porting/AgentGuide.md:104`** — the no-asserts note, which correctly distinguishes Giri's own
  compiled-out `assert()`s from test programs' `assert()`s.
- **Invariant 1 and 3** — thinner evidence than the note asked for (summarised, not pasted: counts
  like "test2: 5 BBs, 20 LS points", "test16: 8 BBs", rather than the diffs). Not contradicted by
  anything, and test16 is genuinely multi-file (`calc.c` + `struct-ptr.c`), so the multi-file
  requirement was met. Re-running them is **not** in scope — if you want the evidence stronger, say
  so in the register rather than reopening a container session.

## Definition of done

- [x] The fabricated `signal`-handler row is gone from `AGENTS.md`'s `## Known residuals`; the
      SIGKILL row is still there
- [x] `SUMMARY.md`'s per-test verdict table carries the commit per row and the current result where
      it changed, with the original verdicts preserved — paste the resulting table:

      | Test | Variant | Verdict | Commit | Current result | Root cause | Report |
      |------|---------|---------|--------|----------------|------------|--------|
      | test3 | seq | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 ... | [UnitTests-test3.md](UnitTests-test3.md) |
      | test5/8/9/10/11/12/17 | seq | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | PostDominatorFrontier.cpp:37 ... | ... |
      | test16 | seq | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (fixed `3b26ea6`) | TraceFile.cpp:378 ... | ... |
      | matrix_multiply | pthread | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (re-verified `llvm-5-matrix-multiply-verdict`) | ... | ... |
      | pca | pthread | FAIL-BUG | pre-`3b26ea6` | **CLEAN** (re-verified `llvm-5-matrix-multiply-verdict`) | ... | ... |
      | kmeans | pthread | FAIL-HARNESS | pre-`3b26ea6` | **unchanged** (harness enhanced `e194151`) | ... | ... |
      | All CLEAN rows | — | — | — | unchanged | — | — |

- [x] The six suite results are reconciled in one place with the current one identified, linked from
      `AGENTS.md` — section added at `SUMMARY.md:120-134`, pointer added at `AGENTS.md:13`
- [x] `git status --ignored --short test/` shows nothing but `test/_test_logs/`:

      ```
      M test/Makefile.common
      !! test/_test_logs/
      ```

- [x] `clean:` exclusion for benchmark output files recorded: comment added to `Makefile.common:135-136`
      noting that each benchmark's runtime output file (e.g., `matrix_file_out_serial.txt`) is
      deliberately not matched — it is program output, not a build artifact.
- [x] The closeout note's invariant-2 progress-log line says `master`, not `port/llvm-5.0.2`
- [x] `AGENTS.md`'s "Done: `llvm-5-port-closeout`" line accounts for this task: new "Done:" entry
      for `llvm-5-closeout-corrections` added at line 86, "Open:" entry removed
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## Traps

- **No container needed.** Nothing here builds, runs `opt`, or runs the suite. If you find yourself
  wanting one, you have widened the scope — stop and say so instead.
- Do not re-run the suite. The current number (`4cd2451`, 21 PASS / 1 FAIL) is what Defect 3 records;
  a fresh run would only add a seventh row to reconcile.
- Do not touch any `ans-*.txt`, criterion file, or `test/Makefile.common` beyond the `clean:` rule.
- `matrix_multiply-seq` stays the one standing failure. Do not reopen it.
- Deleting gitignored artifacts is file removal, not testing — safe from the devcontainer.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `AGENTS.md` — the register row, the suite-results pointer, the "last task" line
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md` — verdict-table columns, suite-results section
- `test/Makefile.common` — the `clean:` rule only
- `porting/TaskNotes/Tasks/llvm-5-port-closeout.md` — the invariant-2 log wording
- `porting/TaskNotes/Tasks/llvm-5-closeout-corrections.md` (this note — progress log)
- Read-only: everything else, `lib/**`, `include/**`, `runtime/**`, `tools/**` included

## Blocked by

- ~~llvm-5-port-closeout~~

## Progress log

- Corrected four items `llvm-5-port-closeout` ticked without delivering: (1) removed fabricated "signal handlers reinstall" row from `AGENTS.md` Known residuals register; (2) annotated `SUMMARY.md` per-test verdict table with commit and current-result columns — 9 FAIL-BUG rows now carry `**CLEAN** (fixed 3b26ea6)`, 2 pthread rows carry `**CLEAN** (re-verified llvm-5-matrix-multiply-verdict)`; (3) added "Suite results across the port" section to `SUMMARY.md` reconciling all 6 suite results with pointer from `AGENTS.md`; (4) cleaned 21 verification artifacts from test/ (2 test2, 10 test16, 9 matrix_multiply) and recorded benchmark output file exclusion in `clean:` rule. Fixed closeout's invariant-2 wording: `port/llvm-5.0.2` → `master` in `llvm-5-port-closeout.md`. Updated `AGENTS.md`: added "Done: llvm-5-closeout-corrections" entry, removed "Open:" entry.

## Handoff
- branch `agent/llvm-5-closeout-corrections`
Refs: `porting/TaskNotes/Tasks/llvm-5-port-closeout.md`,
`porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/TaskNotes/Tasks/llvm-5-harness-fallout.md`,
`AGENTS.md`, `lib/Giri/TracingNoGiri.cpp`, `runtime/Giri/Tracing.cpp`
