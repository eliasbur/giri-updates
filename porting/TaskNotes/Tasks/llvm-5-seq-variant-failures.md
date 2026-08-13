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

> **Premise update (2026-08-13, head agent, after `llvm-5-harness-fallout` merged as `5fbca9d`).**
> Two of this note's starting assumptions no longer hold. Read this block before the body; where
> they conflict, this block wins.
>
> 1. **The failure is a segfault, not 15 stderr warnings.** On the current branch
>    `matrix_multiply-seq` dies with `Segmentation fault (core dumped)` inside
>    `PostDominanceFrontier::calculate`, and `make` reports `Error 139`. The "15 remaining
>    `Could not find Control-dep`" count comes from a build predating `3b26ea6`; it is a historical
>    symptom count, not the current failure mode. Reproduce it yourself, but this is what it looks
>    like — reproduced here because `test/_test_logs/` is gitignored scratch and will not survive a
>    clean or a fresh clone:
>
>    ```text
>    Start slicing Function:Instruction is defined as matrix_mult:285
>    #3 llvm::PostDominanceFrontier::calculate(llvm::PostDominatorTree const&,
>         llvm::DomTreeNodeBase<llvm::BasicBlock> const*)  (libdgutility.so+0x9a63)
>    #4 llvm::PostDominanceFrontier::calculate(...)                (libdgutility.so+0x9a41)
>    #5 giri::DynamicGiri::ensurePostDomFrontierComputed(llvm::Function&)  (libgiri.so+0xb7ae)
>    #6 giri::DynamicGiri::findExecForcers(llvm::BasicBlock*, std::set<unsigned int...>&)
>    #7 giri::DynamicGiri::findSlice(giri::DynValue&, ...)
>    #8 giri::DynamicGiri::getBackwardsSlice(llvm::Instruction*, ...)
>    #9 giri::DynamicGiri::runOnModule(llvm::Module&)
>    Stack dump:
>    1.  Running pass 'Dynamic Backwards Slice Analysis' on module 'matrix_multiply-seq.all.bc'.
>    Segmentation fault (core dumped)
>    make[1]: *** [matrix_multiply-seq.slice] Error 139
>    ```
>
>    Note frames #3 and #4: `calculate` recursing into itself, two levels deep at the fault. Not a
>    stack overflow.
> 2. **Hypothesis 1 below is already refuted for `matrix_multiply`.** The same log shows
>    `Start slicing Function:Instruction is defined as matrix_mult:285` followed by a crash whose
>    stack is `runOnModule → getBackwardsSlice → findSlice → findExecForcers →
>    ensurePostDomFrontierComputed → calculate`. Reaching `getBackwardsSlice` requires a non-null
>    `Criterion` (`lib/Giri/Giri.cpp:305-309`), so instruction #285 of `matrix_mult` resolves and
>    the criterion is fine. Do not spend a session on it — see "Hypothesis 1" for what is left of it.
>
> The crash, and only the crash, is the blocking question for this task.

## Goal

Give every test the suite **actually runs** an evidence-backed verdict, and resolve the one
remaining suite failure — `matrix_multiply` in its sequential configuration — either by fixing the
defect behind it or by proving it is a legitimate 3.4 → 5.0.2 consequence. That second escape does
**not** apply to the current failure: a segfault in Giri's own code is a defect in every case, and
"LLVM 5 does it differently" is a description of the cause, never a verdict.

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

## Start here — the crash

`PostDominanceFrontier::calculate` (`lib/Utility/PostDominatorFrontier.cpp:31-58`) is recursive and
opens with

```cpp
DomSetType &S = Frontiers[BB];   // reference into the map
...
const DomSetType &ChildDF = calculate(DT, IDominee);   // recursion inserts new keys
...
S.insert(*CDFI);                 // writes through the reference afterwards
```

`Frontiers` is declared `DenseMap<BasicBlock*, DomSetType>`
(`include/Utility/PostDominanceFrontier.h:28,65`). **`DenseMap` invalidates every reference into it
when it grows**; `std::map` does not. The port introduced that container — check
`git show master:include/Utility/PostDominanceFrontier.h`, where `Frontiers` is inherited from
LLVM 3.4's `DominanceFrontierBase`, and confirm which container 3.4 used before you rely on this.
If the port swapped a reference-stable map for `DenseMap` while keeping an algorithm that holds a
reference across insertions, `S` dangles as soon as the recursion grows the map past a rehash
threshold, and `S.insert()` writes into freed storage.

That theory also explains the shape of the evidence, which is why it is first:

- The recursion was **dead code until `3b26ea6`**. Before that commit `calculate` began with
  `if (!BB) return S;`, and the root of a multi-exit `PostDominatorTree` is the virtual root with
  `getBlock() == nullptr` — so it returned before ever recursing. `3b26ea6` replaced that with
  `if (BB) { … }` and enabled the recursion for the first time. A latent hazard in the recursive
  body would become reachable at exactly that commit, and the suite has had one run of exposure
  since.
- It fires on `matrix_mult` and not on the unit tests: a rehash needs enough basic blocks in one
  function's post-dominator tree, and the unit tests are small.

Establish the mechanism before changing anything:

- Confirm the crash is reproducible and get a faulting address —
  `docker exec giri-seq make -s -C /giri/test/matrix_multiply TEST_PARALLELISM=seq DEBUGFLAGS=`.
- Record how many basic blocks `matrix_mult` has in the 5.0.2 build, and whether the crash point
  correlates with a `DenseMap` growth boundary. `matrix_multiply-pthread` passes — record its
  `matrix_mult` block count too, as the contrasting data point.
- Second candidate, cheaper to rule out than to prove: `DT.getNode(*CDFI)` returns `nullptr` for a
  block absent from the post-dominator tree, and `properlyDominates` is then called with a null
  node. Check whether LLVM 5.0.2's `DomTreeBase::properlyDominates(const DomTreeNodeBase*, const
  DomTreeNodeBase*)` null-guards its arguments; if it does, this candidate is dead and the
  dangling-reference theory is what is left.

A fix that makes the crash go away without naming the mechanism is not acceptable here — this code
was already "fixed" once, and the fix is what exposed the crash.

## Then these two — but only after the crash is understood

### Hypothesis 1 — the criterion no longer points where it did in 3.4

**Refuted for `matrix_multiply-seq`** — see the premise-update block at the top; `matrix_mult:285`
resolves and slicing starts. What survives is the *general* question for the other two seq
variants, and it is cheap: `pca-seq` (`calc_mean 51`) and `kmeans-seq` (`main 120`) are both scored
PASS, so record for each whether the criterion resolves and to which instruction, as evidence in
their reports that instruction indices are broadly stable across 3.4 → 5.0.2. Do not re-open the
matrix_multiply case unless the crash investigation contradicts the log.

<details>
<summary>Original hypothesis text, kept for the reasoning about `-criterion-inst`</summary>

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

</details>

### Hypothesis 2 — a residual control-dependence defect

Once slicing runs to completion, whatever `Could not find Control-dep of this Basic Block`
(`Giri.cpp:153`) messages the **current** build emits are the material here. Do not go looking for
15 of them — that count is from a pre-`3b26ea6` build and there is no reason to expect it to
survive either that fix or the crash fix. Count what the fixed build actually prints; if it prints
none, say so and close the hypothesis.

For each occurrence that does appear, record the function, the basic block (label and `-bbnum` ID)
and what `findExecForcers` / `Trace->getExecForcer` returned. Suspects, in order:

- Blocks whose post-dominance frontier is genuinely empty (no control dependence) — the message
  may be benign for those, in which case say so with evidence rather than "fixing" it.
- Unreachable blocks, which are absent from the `PostDominatorTree` and therefore never receive a
  frontier entry. Note the overlap with the null-`getNode` candidate above: the same class of block
  is implicated in both.
- `DynamicGiri::ensurePostDomFrontierComputed` (`lib/Giri/Giri.cpp:67`) constructs a
  `PostDominatorTreeWrapperPass` with `new` and runs it outside any pass manager, caching one tree
  and one frontier per function forever (and leaking both). The audit checked this construction on
  the pthread path; re-check it here, including whether the cache survives the
  `-mergereturn`/`-bbnum`/`-lsnum` pipeline intact.

## Definition of done

- [ ] Image built once, one long-lived container for the whole task; no per-test image rebuilds
- [ ] `matrix_multiply-seq`, `pca-seq` and `kmeans-seq` each have a report at
      `porting/TestAudit/llvm-5.0.2/<name>-seq.md`, following the report schema in
      `llvm-5-test-audit.md` verbatim (section order included)
- [ ] The `PostDominanceFrontier::calculate` segfault root-caused to a named mechanism, with
      evidence — not "changed X and it stopped crashing". If the fix is a container or algorithm
      change, state why the previous shape was wrong and why `3b26ea6` made it reachable
- [ ] The relationship to `3b26ea6` recorded: whether that commit introduced the crash by enabling
      recursion that had never run, and whether its own fix (10 tests recovered) is preserved —
      the full suite is the check for that, not inspection
- [ ] Hypothesis 1 answered for `pca-seq` and `kmeans-seq` (does the criterion resolve, to which
      instruction); for `matrix_multiply-seq` the premise-update block's refutation is quoted with
      its log evidence rather than re-derived
- [ ] Every `Could not find Control-dep` occurrence the **current** build emits attributed to a
      function + BB ID and given a verdict; none left as "same as the others". If the count is zero
      after the crash fix, that is the answer and it is recorded as such
- [ ] `matrix_multiply-seq` produces an empty diff against `ans-inst-seq.txt`, **or** carries a
      `FAIL-EXPECTED` verdict showing why the golden line set cannot be produced without changing
      the criterion or the golden file
- [ ] The pthread variants re-verified after any code change: `matrix_multiply-pthread` and
      `pca-pthread` diff empty (both were verified clean in `llvm-5-test-fixes`)
- [ ] Full suite re-run and the per-test result recorded against the 21 PASS / 1 FAIL table in
      `llvm-5-harness-fallout.md`; no test that passed there regresses
- [ ] `SUMMARY.md` corrected: the two "Unresolved questions" resolved (or the variant explanation
      refuted with evidence), and the per-test table annotated with which variant each report covers
- [ ] Evidence quoted into the reports, not cited by path: `test/_test_logs/` is gitignored scratch
      that the next suite run overwrites and `make clean -C test` deletes
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

The harness is trustworthy as of `5fbca9d`: stage output is captured per test under
`test/_test_logs/`, stage markers separate build from test, a failing test's artifacts are left in
place, and a traced binary's exit status is checked against a per-test `EXPECTED_EXIT` (default:
must be 0) rather than swallowed. `porting/AgentGuide.md` → "Declaring an acceptable exit status"
documents it. You should not need to run stages by hand; if you do, say why in the progress log.

One caveat while `llvm-5-harness-residuals` is open: `EXIT_UNCHECKED=1` (only `test9` uses it)
sends the traced binary's output to `/dev/null`. Do not read an empty `UnitTests_test9.log` as
evidence of anything.

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
- `lib/Utility/PostDominatorFrontier.cpp`, `include/Utility/PostDominanceFrontier.h`,
  `lib/Giri/Giri.cpp` — only if a defect is proven. The header is in scope because `Frontiers`'
  container type is declared there.
- `porting/TaskNotes/Tasks/llvm-5-seq-variant-failures.md` (this note — progress log)
- Read-only: `test/**` (all of it, including Makefiles and goldens), `CMakeLists.txt`, `Dockerfile`,
  `utils/**`

## Blocked by

- ~~llvm-5-test-fixes~~
- ~~llvm-5-harness-honesty~~
- ~~llvm-5-harness-fallout~~

Not blocking, but do not run concurrently: `llvm-5-harness-residuals` edits
`test/Makefile.common`, which scores every test.

## Progress log

## Handoff
- branch `agent/llvm-5-seq-variant-failures`
Refs: `porting/TestAudit/llvm-5.0.2/SUMMARY.md`, `porting/TaskNotes/Tasks/llvm-5-test-audit.md`,
`porting/TaskNotes/Tasks/llvm-5-test-fixes.md`, `AGENTS.md`, `test/Makefile.common`
