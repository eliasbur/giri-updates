---
title: Make Giri run unattended on ARVO containers for the LLVM 5 port.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-31
---

## Goal

Turn the half-scripted ARVO pipeline in `arvo/` into a driver that takes a container and a PoC and
produces a slice without a human in the loop, on `port/llvm-5.0.2` only.

## Why this task exists

A previous agent validated the pipeline end to end on `arvo-42473917-vul` (ffmpeg SAMI decoder,
heap-buffer-overflow at `libavcodec/htmlsubtitles.c:174`) and wrote it up in `arvo/README.md`,
`arvo/AUTOMATION.md` and `arvo/RESULTS-sample-container.md`. Nine stages exist as scripts; the
work stopped short of a driver. `arvo/AUTOMATION.md` lists exactly what blocks unattended
operation, and this task closes those items.

Everything in `arvo/` is currently **untracked** — it has never been committed. Part of this task
is landing it.

### The five blockers, as that document states them

1. **Toolchain gate** — genuinely per-container; must be resolved before anything else.
2. **Link-line lookup after a rename** — `giri-cc` keys recorded link lines by output basename, and
   build systems rename the binary afterwards. The fallback ("use the sole recorded link line")
   does not fire when 160 links were recorded. Fix named in the document: stamp the link-line path
   into the binary with `objcopy --add-section .giri_link`, mirroring the proven `.llvm_bc`
   mechanism, and read it back in `giri-instrument`.
3. **No wrapper around the traced run** — nothing runs the traced binary or checks its output. A
   process killed mid-run leaves a trace with no `ENType` terminator, and Giri's trace scans have
   no bound check, so the slicer segfaults on such a file.
4. **Criterion selection** — the one step needing judgement. The document proposes ranking by
   opcode and notes that "a `getLastDynValue()` that reported the miss explicitly would make this a
   proper gate".
5. **Target selection when a build produces many fuzzers** — the sample run edited `$SRC/build.sh`
   to honour `GIRI_CODECS`, the one place the no-edit rule was broken, and it was done for time,
   not necessity.

### Decisions taken with the researcher before starting

- The four Giri fixes in `arvo/000*.patch` **land as commits** on this branch. Pipeline stage 2
  ("apply patches") and its "port branch has diverged" failure mode disappear. The suite baseline
  (21 PASS / 1 FAIL, `matrix_multiply-seq` `FAIL-EXPECTED`) must be re-measured and hold.
- Criterion selection gets **both** halves: ASan-report-driven ranking *and* a fifth Giri change so
  that a criterion which never executed is reported explicitly rather than inferred from slice size.
- Validation is a **full clean end-to-end run** in `arvo-42473917-vul`, not a replay against the
  artifacts already sitting in `/giri`.
- Blocker 5 keeps the no-edit rule as the **default**, with an opt-in escape hatch that backs up
  and restores `$SRC/build.sh`. This was a routine call, not a researcher decision.

## Definition of done

- [ ] `arvo/` is committed, with the four patches landed as commits and the patch files removed.
- [ ] Test suite re-measured on this branch after the patches land; result recorded here and in
      `porting/TestAudit/llvm-5.0.2/SUMMARY.md`.
- [ ] `.giri_link` section stamped by `giri-cc` and read by `giri-instrument`; a rename no longer
      needs `GIRI_LINKCMD`.
- [ ] `giri-trace` exists and enforces both trace gates (no `[GIRI] Abnormal termination`; last
      32-byte record is `ENType`).
- [ ] Giri reports an unexecuted criterion explicitly; `giri-slice` surfaces it as a distinct
      status.
- [ ] Criterion selection is automatic from the ASan report, with ranked fallback candidates.
- [ ] `giri-arvo` drives stages 0-9 unattended against a named container.
- [ ] Full clean end-to-end run on `arvo-42473917-vul` reproduces the documented slice.
- [ ] `arvo/AUTOMATION.md` and `arvo/README.md` updated to describe what is now automated.
- [ ] PR opened into `port/llvm-5.0.2` and linked below.

## Files / scope

- `arvo/` — all scripts and docs (new to git).
- `lib/Giri/TracingNoGiri.cpp`, `runtime/Giri/Tracing.cpp`, `lib/Giri/TraceFile.cpp`,
  `lib/Giri/Giri.cpp`, `include/Giri/TraceFile.h` — the four patches.
- `lib/Giri/TraceFile.cpp`, `include/Giri/TraceFile.h`, `lib/Giri/Giri.cpp` — the criterion
  miss-report gate.
- `AGENTS.md` — current state and residuals.
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md` — suite result after the patches land.

## Blocked by

- (nothing)

## Progress log

## Handoff

- branch `agent/llvm-5-arvo-automation`
Refs: `arvo/AUTOMATION.md`, `arvo/README.md`, `arvo/RESULTS-sample-container.md`,
`porting/AgentGuide.md`
