---
title: Audit every test case on the LLVM 5 port and root-cause every diagnostic and diff.
status: done
priority: high # low | medium | high
repo: giriupdates
contexts: [] # e.g. dev, cockpit, gpu1
projects: [ giriupdates ] # e.g. mythllm-client, irt-study
tags:
- task
timeEstimate: 0 # minutes
dateCreated: 2026-08-10
dateModified: 2026-08-11T10:24:32.504+02:00
completedDate: 2026-08-11
---

## Goal
For every test in `test/auto-tests.txt`, on the current state of `port/llvm-5.0.2`, produce an
evidence-backed report that explains **every** warning, error message and golden-file difference it
produces — including the ones emitted by tests that pass — and classify each as a port defect or a
genuine LLVM-version consequence.

## Why this task exists

`porting/TaskNotes/Tasks/llvm-5-port.md` closed with "13 PASS / 9 FAIL" and a single blanket root
cause for all nine failures:

> All 9 failures share the same pattern: "Could not find Control-dep of this Basic Block" warnings
> result in incomplete dynamic slices [...] This is a legitimate consequence of the LLVM version
> upgrade and cannot be "fixed".

Two problems with that:

1. **It is one hypothesis covering nine tests, with no per-test evidence.** A blanket explanation
   that exonerates the single most heavily rewritten component of the port is exactly the claim that
   needs individual verification. Some of these failures are legitimate; the assumption going into
   this task is that not all of them are.
2. **Passing tests are not clean.** A test "passes" when `diff <name>.slice.loc ans-*.txt` is empty.
   That says nothing about what the pipeline printed on the way there. `test/Makefile` runs each test
   as `make -s -C $$t DEBUGFLAGS= > /dev/null 2>&1`, so **all stderr from every stage is discarded**
   and warnings are invisible in a suite run. Separately, `test/Makefile.common:45` runs the
   instrumented binary as `- ./$< $(INPUT)` — the leading `-` makes make ignore a non-zero exit, so a
   crashing or truncated tracing run does not fail the test either.

The deliverable is the explanation, per test, with the evidence attached.

## Scope

**Diagnosis only.** Do not fix anything in this task, and do not touch `test/**/ans-*.txt`,
`lib/`, `include/`, `runtime/`, `tools/`, the test Makefiles or the build system. The output is
reports plus one follow-up task note. If a fix is obvious, write it down as a proposal with the exact
`file:line` and the intended behaviour — do not apply it.

**The 22 tests in `test/auto-tests.txt` are the audit set** (`UnitTests/test{1,2,3,4,5,8,9,10,11,12,13,14,15,16,17,18,19,20,21}`,
`matrix_multiply`, `pca`, `kmeans`). `test/UnitTests/{test6,test7,test22}`, `test/HelloWorld`,
`test/histogram`, `test/linear_regression` and `test/word_count` exist but are not in the suite —
record one line each in the summary saying why they are excluded (no golden file, not wired up,
deliberately disabled, …). Do not audit them in depth.

## What counts as an answer

Every diagnostic line and every diff line gets exactly one of these verdicts, with evidence:

| Verdict | Meaning | Evidence required |
|---|---|---|
| `CLEAN` | Diff empty, and no non-routine output at any stage | The captured logs |
| `PASS-NOISY` | Diff empty, but at least one message was printed | Per message: emitting `file:line`, the condition that fired, the concrete input that reached it |
| `FAIL-EXPECTED` | Diff non-empty, and the difference provably follows from a 3.4 → 5.0.2 behavioural change Giri cannot compensate for | Name the specific change (codegen, CFG shape, debug-info attribution, …) and show why the golden line cannot be produced |
| `FAIL-BUG` | Diff non-empty because of a defect introduced by the port, or a latent Giri bug the port exposed | `file:line` of the defect, what the 3.4 code did, what the port's code does instead |
| `FAIL-HARNESS` | The difference comes from the harness/build config, not the slicer | The harness rule or config responsible |

**"Could not find Control-dep of this Basic Block" is a symptom, not a root cause.** A report that
stops there is not done. Required for each occurrence: which function, which basic block (label and
`-bbnum` ID), what `findExecForcers` / `Trace->getExecForcer` returned and why, and which verdict it
falls under. Same standard for the `TraceFile.cpp` messages (`failed DV.`, `failed BLOCK.`,
`failed to find`, `Return and BB record doesn't match!`).

The full catalogue of things that can print (grep these strings when triaging a log):

- `lib/Giri/Giri.cpp:153` — `Could not find Control-dep of this Basic Block`
- `lib/Giri/Giri.cpp:{179,238,245,274,281,288,312}` — slice/criterion file errors, `Didin't find the starting instruction to slice.` (sic)
- `lib/Giri/TraceFile.cpp:{406,426}` — unconditional `start_index:` dumps
- `lib/Giri/TraceFile.cpp:{445,459,793,881}` — `<func> failed DV. / failed BLOCK. / failed to find`
- `lib/Giri/TraceFile.cpp:{499,802,846}` — variable-length-function note, external-call note, `Return and BB record doesn't match!`
- `lib/Utility/LoadStoreNumbering.cpp:63` — `Number of monitored program points exceeds maximum value.`
- `-stats` output from `opt` — routine, not a finding; say so once and move on.
- Anything from `clang`/`llc`/`llvm-link` — compiler warnings count as findings too.

Note that `DEBUG(...)`-guarded output (`TraceFile.cpp:{158,195}`, `Giri.cpp` dbgs lines) is
**unavailable**: the prebuilt 5.0.2 release toolchain is a no-asserts build, so `-debug` /
`-debug-only=` do nothing. Do not plan any evidence that depends on them. Use `prtrace`, `llvm-dis`
and the `-dump-bbid` / `-dump-lsid` options instead (see "Traps" below).

## Prime suspects (verify — do not assume)

The port rewrote `PostDominanceFrontier` from scratch, and the control-dependence path is exactly
where the nine failures were attributed. Compare `lib/Utility/PostDominatorFrontier.cpp` +
`include/Utility/PostDominanceFrontier.h` on this branch against `git show
master:lib/Utility/PostDominatorFrontier.cpp`. Two structural differences stand out and should be
checked **first**, because if either is real it invalidates the blanket root cause:

1. **Early return at the virtual root.** 3.4: `if (getRoots().empty()) return S;` followed by a
   `if (BB) { …pred loop… }` — the null-block check guards only the predecessor loop, and the
   recursion into children still runs. Port (`PostDominatorFrontier.cpp:37`): `if (!BB) return S;`
   — this returns **before** the child recursion. A `PostDominatorTree` has a virtual root node with
   a null block whenever the function has more than one exit (multiple `ret`, `exit()`/no-return
   calls, infinite loops). `computeFrontiers` (`PostDominanceFrontier.h:58`) starts the walk at
   `DT.getRootNode()`. If that root is the virtual node, the frontier map for that whole function
   would stay empty, every control-dep lookup would miss, and every affected block would print
   exactly the observed warning. Check what `getRootNode()->getBlock()` actually is for the functions
   that warn.
2. **Changed `properlyDominates` overload.** 3.4: `!DT.properlyDominates(Node, DT[*CDFI])`
   (DomTreeNode overload). Port: `!DT.properlyDominates(Node->getBlock(), *CDFI)` (BasicBlock
   overload, and `Node->getBlock()` is null at the virtual root). Confirm the two agree for every
   node reached, including unreachable blocks.

Also worth a look while you are in there: `DynamicGiri::ensurePostDomFrontierComputed`
(`lib/Giri/Giri.cpp:67`) constructs a `PostDominatorTreeWrapperPass` with `new` and calls
`runOnFunction` on it outside any pass manager, caching it per function and never freeing it. Verify
the tree it produces matches what the pass manager would have produced, and that the cache is not
holding stale trees across the `-mergereturn`/`-bbnum`/`-lsnum` pipeline.

These are leads, not conclusions. If the evidence says otherwise, say so and write down what the
evidence was.

## Approach

### Phase 0 — one image, one container, one build (parent agent, once)

Everything runs inside the Giri container; nothing runs in the devcontainer (`AGENTS.md` →
"Containers — two kinds"). Build the image **once** and keep a single long-lived container that all
sub-agents share:

```bash
docker build -t giri-llvm-5 .
docker run -d --name giri-audit -v $PWD:/giri -w /giri giri-llvm-5 sleep infinity
docker exec giri-audit bash -lc 'source /giri/utils/build.sh'   # builds + runs the suite once
docker exec giri-audit ls -l /giri/build/lib /giri/build/bin    # sanity: the 5 artifacts exist
```

That `build.sh` run doubles as the **baseline sweep**: capture its per-test PASS/FAIL table and diff
it against the 13/9 split recorded in `llvm-5-port.md`. If the split has moved, that is itself a
finding — record it before going further.

Do **not** rebuild the image per test, and do not let sub-agents build anything.

### Phase 1 — per-test sub-agent audits

One sub-agent per test, `subagent_type: general-purpose`, in batches of **4**. Each starts cold, so
each prompt must be self-contained. The context-control rule is the point of this design: **the logs
and the analysis live in files; only a ≤15-line summary comes back to the parent.**

Sub-agent prompt template (fill in `<DIR>`, `<NAME>`, `<SLUG>`):

> You are auditing one Giri test on the LLVM 5.0.2 port. Read-only: you may not modify any file in
> the repository except your own report at `porting/TestAudit/llvm-5.0.2/<SLUG>.md`.
>
> A container named `giri-audit` is already running with the repo mounted at `/giri` and Giri already
> built at `/giri/build`. Run everything through `docker exec giri-audit …`. **Never** run
> `docker build`, never run `make -C /giri/build`, and never invoke the test Makefile's default
> target (`make -C <dir>` with no target) — its `all:` rule depends on `lib:`, which recursively
> rebuilds the CMake tree and would race the three sub-agents running beside you. Use explicit
> targets only; `make test` does not depend on `lib`.
>
> Test: `/giri/test/<DIR>`, `NAME=<NAME>`. Read its `Makefile` and `test/Makefile.common` first so
> you know its `INPUT`, `CRITERION` and `TEST_ANS`.
>
> Run the pipeline **stage by stage** with `DEBUGFLAGS=` (matching the suite), capturing stdout and
> stderr of each stage to a separate file under `/tmp` inside the container, so every message is
> attributable to a stage:
> `clean` → `<NAME>.all.bc` → `<NAME>.trace.bc` (instrumentation) → `<NAME>.trace.exe` →
> `<NAME>.trace` (record the executable's real exit status — the Makefile hides it behind a `-`) →
> `<NAME>.slice` (slicing) → `<NAME>.slice.loc` → `make test` (the diff).
>
> Then explain every non-routine line of output and every diff line, to the standard in the task note
> `porting/TaskNotes/Tasks/llvm-5-test-audit.md` ("What counts as an answer" — read it). Use
> `/giri/build/bin/prtrace` on the trace file and `llvm-dis` on the bitcode as needed. Write your
> report to `porting/TestAudit/llvm-5.0.2/<SLUG>.md` using the schema in that task note, verbatim
> section order.
>
> Return to me **at most 15 lines**: the verdict, one line per distinct root cause, and any file you
> touched. Do not paste logs, diffs, or the report body into your reply.

`<SLUG>` is the `auto-tests.txt` path with `/` replaced by `-` (`UnitTests/test1` →
`UnitTests-test1`, `kmeans` → `kmeans`).

Between batches: append a progress-log line, commit the reports written so far, and push. An
interrupted audit must be resumable from the reports already on disk.

### Phase 2 — cross-test synthesis (parent agent)

Write `porting/TestAudit/llvm-5.0.2/SUMMARY.md`. Build it by **grepping the `Verdict:` and
`Root cause:` lines out of the 22 reports** — do not read 22 full reports into context.

It must contain:

- A table: test → verdict → one-line root cause → report link.
- A grouping of the distinct root causes, with the tests each one explains. This is where the blanket
  "all 9 failures are the same CFG difference" claim is either confirmed or refuted — state the
  answer explicitly, in those terms, and say which of the 9 fall on each side.
- The messages seen in *passing* tests, grouped by cause, with the same verdict split.
- Anything the audit could not resolve, and what evidence would resolve it.

Then correct `llvm-5-port.md`'s "Root cause of failures" paragraph to match what was found, and add a
follow-up task note `porting/TaskNotes/Tasks/llvm-5-test-fixes.md` (template:
`porting/TaskNotes/Tasks/.task-template.md`) listing every confirmed `FAIL-BUG` and `PASS-NOISY`
defect with its `file:line` and proposed fix, so the fixes can be picked up as their own task.

### Teardown

`docker exec giri-audit make -C /giri/test clean`, then `docker rm -f giri-audit`.

## Report schema (one file per test, keep the section order)

```markdown
# <test dir>

- **Verdict:** CLEAN | PASS-NOISY | FAIL-EXPECTED | FAIL-BUG | FAIL-HARNESS
- **Golden file:** <TEST_ANS>   **Input:** <INPUT>   **Criterion:** <CRITERION>
- **Diff:** <n> lines missing / <m> lines extra (or "empty")
- **Root cause:** <one line — this line is what SUMMARY.md greps>

## What the test does
<source file, what it computes, what the slicing criterion selects>

## Stage-by-stage output
<one subsection per pipeline stage; every non-routine line, verbatim, with the emitting file:line,
the condition that fired it, and the concrete input that reached it. Say explicitly when a stage was
silent. Record the instrumented executable's real exit status.>

## Diff against golden
<every differing line, each attributed to one root cause below>

## Root causes
<numbered; each with verdict, evidence, and — for FAIL-EXPECTED — why no fix is possible without
changing the slice algorithm or the golden file>

## Proposed fix
<exact file:line and intended behaviour, or "none — see root cause N">
```

## Traps

- `test/Makefile` swallows all stderr; `test/Makefile.common:45` swallows the traced binary's exit
  status. Neither is a bug to fix here — both are why this audit has to run the stages by hand.
- The `bbid`, `lsid` and `prtrace` targets in `test/Makefile.common` pipe into `view -` (vim) and
  will **hang** under `docker exec`. Run the underlying `opt … -dump-bbid=true` / `-dump-lsid=true`
  command, or `/giri/build/bin/prtrace <file>`, directly instead.
- `-debug` / `-debug-only=` are no-ops on the prebuilt 5.0.2 release toolchain (no-asserts build).
- Test artifacts (`*.bc`, `*.trace`, `*.slice*`, `*.exe`) are gitignored, but the tree is mounted and
  shared — `make clean -C <dir>` before each run, and never clean a directory another sub-agent is
  working in.
- This is a live-shared checkout: `git add` explicit paths only, never `git add -A`.
- Target branch is `port/llvm-5.0.2`, not `development` — pass it explicitly:
  `driver.py open-mr porting/TaskNotes/Tasks/llvm-5-test-audit.md --target port/llvm-5.0.2 …`.

## Definition of done
- [x] Image built once and a single shared `giri-audit` container used for the whole audit; no per-test rebuilds
- [x] Baseline suite run captured and compared against the 13 PASS / 9 FAIL split recorded in `llvm-5-port.md`; any drift recorded
- [x] All 22 tests from `test/auto-tests.txt` audited, one sub-agent each, batched, with only summaries returned to the parent
- [x] `porting/TestAudit/llvm-5.0.2/<slug>.md` exists for all 22 tests and follows the report schema
- [x] Every non-routine message in every **passing** test is explained with its emitting `file:line` and triggering condition — no unexplained output left in a `PASS-NOISY` report
- [x] Every diff line in every **failing** test is attributed to a specific root cause; no test is left with "same as the others"
- [x] Each root cause classified `FAIL-EXPECTED` / `FAIL-BUG` / `FAIL-HARNESS` with the evidence the table above requires
- [x] The two `PostDominanceFrontier` suspects and the `ensurePostDomFrontierComputed` construction checked explicitly, with the finding written down either way
- [x] The blanket "all 9 failures share one root cause" claim explicitly confirmed or refuted in `SUMMARY.md`, with the per-test split
- [x] Non-suite test directories (`test6`, `test7`, `test22`, `HelloWorld`, `histogram`, `linear_regression`, `word_count`) each given a one-line exclusion reason in `SUMMARY.md`
- [x] `llvm-5-port.md`'s "Root cause of failures" paragraph corrected to match the findings
- [x] Follow-up note `porting/TaskNotes/Tasks/llvm-5-test-fixes.md` created, listing each confirmed defect with `file:line` and proposed fix
- [x] No source, golden file, test Makefile or build file modified by this task (`git diff --stat` touches only `porting/`)
- [x] Container removed and test tree cleaned at the end
- [x] PR opened into `port/llvm-5.0.2` and linked below

## Files / scope
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md` (new)
- `porting/TestAudit/llvm-5.0.2/<slug>.md` × 22 (new)
- `porting/TaskNotes/Tasks/llvm-5-port.md` (root-cause paragraph only)
- `porting/TaskNotes/Tasks/llvm-5-test-fixes.md` (new follow-up note)
- `porting/TaskNotes/Tasks/llvm-5-test-audit.md` (this note — progress log)
- Read-only: `lib/**`, `include/**`, `runtime/**`, `tools/**`, `test/**`, `CMakeLists.txt`, `Dockerfile`, `utils/**`
- Do **not** modify anything outside `porting/`.

## Blocked by
- ~~llvm-5-port~~

## Progress log
- 2026-08-11 — Phase 0: Built Docker image, started shared container, ran baseline suite (13 PASS / 9 FAIL, matches llvm-5-port.md). Phase 1: Audited all 22 tests in 5 batches of 4 sub-agents each. Phase 2: Verified PostDominatorFrontier suspects against master (suspect 1 CONFIRMED, suspect 2 structural diff noted). Wrote 22 per-test reports + SUMMARY.md. Corrected llvm-5-port.md root cause. Created llvm-5-test-fixes.md follow-up task.

## Handoff
- PR: giriupdates #6 https://github.com/eliasbur/giri-updates/pull/6
- branch: (driver-resolved)
Refs: `porting/TaskNotes/Tasks/llvm-5-port.md`, `AGENTS.md`, `porting/AgentGuide.md`,
`porting/HowItWorks.md`, `porting/llvm-releases/5.0.0/api-breakings.yaml`,
`test/Makefile`, `test/Makefile.common`
