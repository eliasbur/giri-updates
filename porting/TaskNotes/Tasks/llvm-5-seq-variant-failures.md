---
title: Root-cause the sequential-variant test failures the audit never covered.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-12
---

> **Note from `llvm-5-harness-fallout` (2026-08-12):** The "15 remaining \`Could not find Control-dep\`"
> diagnosis is only half of the story. On the current `port/llvm-5.0.2`, `matrix_multiply-seq`
> actually **segfaults** during slicing in `PostDominanceFrontier::calculate` (null \`BasicBlock*\`
> inserted into an \`std::set\`, stack trace in \`_test_logs/matrix_multiply.log\`). The stderr
> "Could not find Control-dep" output was from earlier runs before this crash path manifested, or
> may appear on different builds. Verify the actual failure mode before assuming it is a benign
> control-dependence warning.

## Goal

Give every test the suite **actually runs** an evidence-backed verdict, and resolve the one
remaining suite failure — `matrix_multiply` in its sequential configuration — either by fixing the
defect behind it or by proving it is a legitimate 3.4 → 5.0.2 consequence.

## Why this task exists

`Dockerfile:5` sets `ENV TEST_PARALLELISM=seq`. The three benchmark Makefiles
(`test/{matrix_multiply,pca,kmeans}/Makefile`) select their variant with `TEST_PARALLELISM ?= pthread`,
and `?=` yields to an environment variable — so inside the Giri container the suite builds
`matrix_multiply-seq`, `pca-seq` and `kmeans-seq` and diffs against `ans-inst-seq.txt`.

The audit (`porting/TestAudit/llvm-5.0.2/`) analysed the **pthread** variants of all three: every
one of those three reports names `ans-inst-pthread.txt` as its golden file. The seq variants — the
ones the suite scores — have never been audited, and no report exists for the failure that is still
open:

> Full suite (22 tests): 21 PASS, 1 FAIL (matrix_multiply seq version, which fails because
> `TEST_PARALLELISM=seq` is hardcoded in the Dockerfile and the seq version has 15 remaining
> "Could not find Control-dep" errors)
> — `llvm-5-test-fixes.md`, progress log

Two consequences:

1. The last failing test is undiagnosed. "15 remaining control-dep errors" is a symptom count, not
   a root cause, and the same standard as the audit applies (`llvm-5-test-audit.md` → "What counts
   as an answer").
2. The variant split is almost certainly the answer to **both** "Unresolved questions" in
   `porting/TestAudit/llvm-5.0.2/SUMMARY.md` — pca and kmeans were scored PASS by the baseline
   sweep (seq) and FAIL by the audit (pthread). The summary attributes this to a possible race or
   stale artifacts. Confirm the variant explanation and correct it.

## Check these two hypotheses before changing any code

### Hypothesis 1 — the criterion no longer points where it did in 3.4

`-criterion-inst` is documented as "Define slicing criterion by instruction number"
(`lib/Giri/Giri.cpp:50`) and resolved at `lib/Giri/Giri.cpp:296-303`:

```cpp
for (inst_iterator I = inst_begin(Func), E = inst_end(Func); I != E; ++I)
  if (--StartInst == 0) { Criterion = &*I; break; }
```

The number is the **N-th LLVM instruction of that function**, not a source line.
`test/matrix_multiply/criterion-inst-seq.txt` is `matrix_mult 285`, and `matrix_multiply-seq.c` has
only 180 lines — which proves the reading. Clang 5.0.2 does not emit the same instruction sequence
as clang 3.4 even at `-O0`, so instruction #285 of `matrix_mult` may now be a different instruction
than the one the golden file was generated from, and the walk falls off the end silently
(`Criterion` stays null → `Didin't find the starting instruction to slice.` at `Giri.cpp:312`).

Settle this **first**, because if the criterion resolves to the wrong instruction the diff is
`FAIL-EXPECTED` and no amount of control-dependence fixing will close it:

- `llvm-dis` the `.all.bc` and identify instruction #285 of `matrix_mult` in the 5.0.2 build.
- Compare against what the golden slice implies the criterion should be (the golden covers lines
  56-154 of `matrix_multiply-seq.c`; the criterion should be the store of a computed cell).
- Record the instruction count of `matrix_mult` — if it is below 285 the criterion never resolves.

The same question applies to `pca-seq` (`calc_mean 51`) and `kmeans-seq` (`main 120`); both are
currently scored PASS, so if their criteria resolve correctly that is evidence instruction indices
are broadly stable and the matrix_multiply case is specific.

### Hypothesis 2 — a second control-dependence defect

If the criterion is fine, the 15 `Could not find Control-dep of this Basic Block` messages
(`Giri.cpp:153`) are a real defect that the virtual-root fix did not cover. For each occurrence,
record the function, the basic block (label and `-bbnum` ID) and what `findExecForcers` /
`Trace->getExecForcer` returned. Suspects, in order:

- Blocks whose post-dominance frontier is genuinely empty (no control dependence) — the message
  may be benign for those, in which case say so with evidence rather than "fixing" it.
- Unreachable blocks, which are absent from the `PostDominatorTree` and therefore never receive a
  frontier entry.
- `DynamicGiri::ensurePostDomFrontierComputed` (`lib/Giri/Giri.cpp:67`) constructs a
  `PostDominatorTreeWrapperPass` with `new` and runs it outside any pass manager, caching one tree
  and one frontier per function forever. The audit checked this construction on the pthread path;
  re-check it here, including whether the cache survives the `-mergereturn`/`-bbnum`/`-lsnum`
  pipeline intact.
- `PostDominanceFrontier::calculate`'s `properlyDominates(Node, DT.getNode(*CDFI))` path
  (`lib/Utility/PostDominatorFrontier.cpp:52`), which only became reachable with the virtual-root
  fix and has therefore had exactly one test-suite run of exposure.

## Definition of done

- [ ] Image built once, one long-lived container for the whole task; no per-test image rebuilds
- [ ] `matrix_multiply-seq`, `pca-seq` and `kmeans-seq` each have a report at
      `porting/TestAudit/llvm-5.0.2/<name>-seq.md`, following the report schema in
      `llvm-5-test-audit.md` verbatim (section order included)
- [ ] Hypothesis 1 answered with evidence for `matrix_multiply-seq`: what instruction #285 of
      `matrix_mult` actually is in the 5.0.2 build, and whether the criterion resolves at all
- [ ] Each of the 15 `Could not find Control-dep` occurrences attributed to a function + BB ID and
      a verdict; none left as "same as the others"
- [ ] `matrix_multiply-seq` produces an empty diff against `ans-inst-seq.txt`, **or** carries a
      `FAIL-EXPECTED` verdict showing why the golden line set cannot be produced without changing
      the criterion or the golden file
- [ ] The pthread variants re-verified after any code change: `matrix_multiply-pthread` and
      `pca-pthread` diff empty (both were verified clean in `llvm-5-test-fixes`)
- [ ] Full suite re-run and the per-test result recorded; no test that passed before regresses
- [ ] `SUMMARY.md` corrected: the two "Unresolved questions" resolved (or the variant explanation
      refuted with evidence), and the per-test table annotated with which variant each report covers
- [ ] No golden file, criterion file or test Makefile modified — if one of them has to change,
      **stop and escalate** rather than editing it
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"):

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-seq -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-seq bash -lc 'source /giri/utils/build.sh'
```

Rebuild with `make -j$(nproc) -C /giri/build`, not `docker build`. Remove the container
(`docker rm -f giri-seq`) and clean the test tree (`make -C /giri/test clean`) when done.

Run a single variant explicitly — a command-line assignment overrides both the Makefile default and
the container environment:

```bash
docker exec giri-seq make -s -C /giri/test/matrix_multiply TEST_PARALLELISM=seq DEBUGFLAGS=
docker exec giri-seq make test -s -C /giri/test/matrix_multiply TEST_PARALLELISM=seq DEBUGFLAGS=
```

`llvm-5-harness-honesty` runs before this task and should have made the suite's PASS/FAIL line
trustworthy. Confirm that it did — check that `test/Makefile` no longer discards stage output and
that `test/Makefile.common:45` no longer swallows the traced binary's exit status — before relying
on it. If for any reason that task has not landed, run the stages by hand and diff by hand, as the
audit did.

## Traps

- **Variant selection.** `TEST_PARALLELISM` is set in the image environment, and `?=` in the test
  Makefiles yields to the environment. Always pass the variant on the `make` command line so the
  run is unambiguous, and name the variant in every report and log filename.
- Never invoke a test directory's default target (`make -C <dir>` with no target) — `all:` depends
  on `lib:` and recursively rebuilds the CMake tree. `make test` does not.
- `make clean -s -C <dir>` between runs; artifacts are gitignored but the tree is shared.
- The `bbid` / `lsid` / `prtrace` targets in `test/Makefile.common` pipe into `view -` (vim) and
  hang under `docker exec` — run `opt … -dump-bbid=true` / `-dump-lsid=true` or
  `/giri/build/bin/prtrace <file>` directly.
- `-debug` / `-debug-only=` are no-ops: the prebuilt 5.0.2 toolchain is a no-asserts build. That
  also means the `assert(Func)` at `Giri.cpp:293` is compiled out.
- **kmeans is only in scope as a seq-variant audit report.** Its pthread crash and its 108 GB trace
  belong to `llvm-5-kmeans`; the harness masking belongs to `llvm-5-harness-honesty`. If
  `kmeans-seq` turns out to be broken too, write the report and hand the fix over there. Start the
  container with `--cpuset-cpus=0-3` so a stray kmeans-pthread run cannot fill the disk.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `porting/TestAudit/llvm-5.0.2/{matrix_multiply,pca,kmeans}-seq.md` (new)
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md` (unresolved questions + variant annotation)
- `lib/Utility/PostDominatorFrontier.cpp`, `lib/Giri/Giri.cpp` — only if a defect is proven
- `porting/TaskNotes/Tasks/llvm-5-seq-variant-failures.md` (this note — progress log)
- Read-only: `test/**` (all of it, including Makefiles and goldens), `CMakeLists.txt`, `Dockerfile`,
  `utils/**`

## Blocked by

- ~~llvm-5-test-fixes~~

## Progress log

## Handoff
- branch `agent/llvm-5-seq-variant-failures`
Refs: `porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/TaskNotes/Tasks/llvm-5-test-audit.md`,
`porting/TaskNotes/Tasks/llvm-5-test-fixes.md`, `AGENTS.md`, `test/Makefile.common`
