---
title: Confirm the matrix_multiply-seq criterion drift by finding the index that reproduces the golden.
status: open
priority: high
repo: giriupdates
contexts: []
projects: [ giriupdates ]
tags:
  - task
timeEstimate: 0
dateCreated: 2026-08-13
---

## Goal

Run the one experiment that settles `matrix_multiply-seq`: find the instruction index that
reproduces `ans-inst-seq.txt` exactly, and refile the verdict on that number instead of on a
reconstruction of LLVM 3.4's instruction counts that the golden file contradicts.

## Why this task exists

`llvm-5-matrix-multiply-verdict` (merged as `6548c10`) did the identification step well and it stands:
`matrix_mult` has **298** instructions under LLVM 5.0.2, instruction **#285 is the `fprintf` at
source line 94** (`dprintf("%d  ", data_in->matrix_out[…])`), the computation store is at **#224**
(line 84), and the 16 extra lines are 15 data-dependence and 1 control-dependence sourced. Those are
real measurements and this task keeps them.

The explanation built on top of them is wrong, and the check that would have caught it was skipped.

### The filed mechanism points the wrong way

> Under LLVM 3.4, the function was shorter (no `dbg.declare` as separate instructions, fewer
> loop-prolog instructions), so #285 resolved to an instruction in the computation loop near the
> `matrix_out` store.

A **fixed** index into a **shorter** instruction stream lands *further along the source program*,
not earlier. If 3.4 emitted fewer instructions before the printing loop, 3.4's #285 would be at or
past line 94 — not back at line 84. For #285 to have been the store at line 84, LLVM 3.4 would have
had to emit **more** instructions before it than 5.0.2 does, which is the opposite of the stated
cause. (The supporting claim is also doubtful on its own: clang 3.4 at `-O0` emits
`call void @llvm.dbg.declare(…)` too. Do not assert anything about 3.4's instruction stream without
a 3.4 build to count in.)

### The golden file refutes it directly

The criterion's own source line appears in its slice — that is how line 94 got into the actual
output. So whatever 3.4's #285 was, **its line is in the golden**. The golden is:

```
56 90 97 113 119 120 121 122 123 129 131 133 137 139 141 145 147 149 154
```

**Line 84 is not in it.** The 3.4 criterion was therefore not the computation store, and the entire
"drifted 61 instructions out of the computation loop" story collapses.

### What the golden actually looks like

Read the golden against the source (`test/matrix_multiply/matrix_multiply-seq.c`):

| line | source | in golden? | in actual? |
|---|---|---|---|
| 84 | `matrix_out[…] += matrix_A[…] * matrix_B[…]` (the store) | no | extra |
| 90 | `for(i = 0; i < matrix_len; i++)` — outer print loop | **yes** | yes |
| 92 | `for(j = 0; j < matrix_len; j++)` — inner print loop | no | extra |
| 94 | `dprintf("%d  ", matrix_out[…])` — value print (**5.0.2's #285**) | no | extra |
| 97 | `dprintf("\n")` — newline print | **yes** | **missing** |

That is precisely the slice of the `dprintf("\n")` at line 97: the criterion's own line (97), the
outer loop it is control-dependent on (90), and nothing from the inner loop or the computation —
because a newline print reads no computed data.

And it explains every element of the diff at once, which the filed story does not:

- **missing 97** — it *was* the 3.4 criterion, and it is not reachable from a criterion at line 94
  (two independent calls).
- **extra 94** — the new criterion's own line.
- **extra 92** — the inner loop the new criterion is control-dependent on.
- **extra 75-86** — the value print reads `matrix_out`, so the whole computation chain enters the
  slice. The newline print reads nothing, so it never did.
- **extra 155-157, 165** — the same data chain continuing back into `main`'s matrix setup.

So the drift is plausibly **a handful of instructions inside one printing loop** — from the newline
print to the value print — not 61 instructions across the function. `FAIL-EXPECTED` may well survive
this; the magnitude and the mechanism do not.

### The decisive experiment was specified and skipped

`llvm-5-matrix-multiply-verdict` step 2 said: sweep nearby indices and find whether some
`matrix_mult N` reproduces the golden exactly. Its own condition was met — #285 turned out to be an
`fprintf`, not the store — and the sweep was not run. It is a loop over ~30 indices.

## What to do

### 1. Sweep

Work in a scratch directory with a copy of the criterion file. **Do not touch
`test/matrix_multiply/criterion-inst-seq.txt`.**

```bash
cd /giri/test/matrix_multiply
make -s TEST_PARALLELISM=seq DEBUGFLAGS=          # build once: .all.bc and .trace must exist
for n in $(seq 270 298); do
  echo "matrix_mult $n" > /tmp/crit-$n.txt
  opt -load /giri/build/lib/libdgutility.so -load /giri/build/lib/libgiri.so \
      -mergereturn -bbnum -lsnum \
      -dgiri -trace-file=matrix_multiply-seq.trace -slice-file=/tmp/slice-$n \
      -criterion-inst=/tmp/crit-$n.txt -remove-bbnum -remove-lsnum \
      matrix_multiply-seq.all.bc -o /dev/null 2>/dev/null
  sed -n '/^Source.*[0-9]\+$/p' /tmp/slice-$n | awk -F: '{print $3}' | sort -g | uniq > /tmp/loc-$n
  printf "%3d  %s\n" "$n" "$(diff -q /tmp/loc-$n ans-inst-seq.txt >/dev/null && echo MATCH || echo "differs ($(diff /tmp/loc-$n ans-inst-seq.txt | grep -c '^[<>]') lines)")"
done
```

Widen the range if nothing hits. Record the whole table, not just the hit — the near-misses are
evidence too.

Expected, if the reading above is right: a match at the index of line 97's `dprintf` call, a few
instructions after 285.

### 2. Report what the sweep says

- **A match** — quantify it: "3.4's `matrix_mult 285` is 5.0.2's `matrix_mult N`, drift of N-285
  instructions, both inside the output-printing loop; the golden is reproducible from the drifted
  index." `FAIL-EXPECTED` is then confirmed with a mechanism that survives inspection, and the
  drift's *cause* can stay unexplained — an unexplained 3.4-to-5.0.2 offset is an honest finding,
  an invented one is not.
- **No match anywhere** — criterion drift is not the explanation, and the diff is a slicing defect.
  Escalate it with the same standard the `DenseMap` fix met; do not file `FAIL-EXPECTED` a third
  time on a story.

### 3. Refile

Rewrite the root-cause section of `porting/TestAudit/llvm-5.0.2/matrix_multiply-seq.md` on the
sweep's result. Keep everything the previous task measured — the 298 count, #285's identity, #224,
the `-trace-cd=false` classification, the corrected line accounting. Remove the claims about 3.4's
instruction stream that no one has measured. Correct the `SUMMARY.md` row and the `AGENTS.md`
`## Current state` paragraph, which currently states the refuted mechanism as settled fact.

## Definition of done

- [ ] Sweep run and the full index→result table recorded in the report, not just the outcome
- [ ] Either the matching index identified and the drift stated as a number, or "no index in range
      N…M reproduces the golden" recorded with the range searched
- [ ] The line-97 / line-94 reading confirmed or refuted against the sweep result
- [ ] Every claim about LLVM 3.4's instruction stream either backed by a 3.4 build that was actually
      counted, or removed. An unexplained offset is an acceptable finding; a reconstructed one is not
- [ ] `matrix_multiply-seq.md` refiled; the previous task's measurements preserved
- [ ] `SUMMARY.md`'s `matrix_multiply | seq` row matches the refiled verdict
- [ ] `AGENTS.md` `## Current state` corrected — it currently asserts the drift-to-the-store story
- [ ] Full suite re-run and recorded: expected **21 PASS / 1 FAIL**, or 22 PASS / 0 FAIL if this
      turns out to be a defect that gets fixed
- [ ] No `ans-*.txt`, no criterion file, no `test/auto-tests.txt` and no test Makefile modified
- [ ] PR opened into `port/llvm-5.0.2` — pass `--target port/llvm-5.0.2` explicitly

## How to build and test

Nothing is built or tested in the devcontainer (`AGENTS.md` → "Containers — two kinds"):

```bash
docker build -t giri:5.0.2 .
docker run -d --name giri-sweep --cpuset-cpus=0-3 -v $PWD:/giri -w /giri giri:5.0.2 sleep infinity
docker exec giri-sweep bash -lc 'source /giri/utils/build.sh'
```

The sweep reuses one `.trace` and one `.all.bc` — build the test once, then loop the `-dgiri` stage
only. Do not run `make` inside the loop; `all:` rebuilds the CMake tree.

If, and only if, the sweep is inconclusive and you need 3.4's actual instruction count:
`utils/install_llvm.sh` still carries a `3.4` case. That is a second image build and hours of work —
justify it in the progress log before starting, and do not do it just to explain an offset the sweep
already quantified.

## Traps

- **Do not re-litigate the `DenseMap` fix or re-measure #285.** Those are settled and correct
  (`6548c10`, `3945134`). This task adds one experiment and rewrites one conclusion.
- Do not edit the golden or the criterion file. Sweeping means writing *scratch* criterion files
  outside the test directory.
- A fixed index into a shorter instruction stream lands *later* in the program, not earlier. That
  inversion is what produced this task; do not reproduce it.
- `-debug` / `-debug-only=` are no-ops on the no-asserts 5.0.2 toolchain, as are Giri's own
  `assert()`s (`Release` CMake build).
- The `bbid` / `lsid` / `prtrace` targets pipe into `view -` (vim) and hang under `docker exec`.
- Evidence goes in the report; `test/_test_logs/` is gitignored scratch.
- Live-shared checkout: `git add` explicit paths only, never `git add -A`.

## Files / scope

- `porting/TestAudit/llvm-5.0.2/matrix_multiply-seq.md` — the refiled verdict
- `porting/TestAudit/llvm-5.0.2/SUMMARY.md` — the `matrix_multiply | seq` row
- `AGENTS.md` — `## Current state`
- `lib/Utility/PostDominatorFrontier.cpp`, `lib/Giri/Giri.cpp` — only if the sweep proves a defect
- `porting/TaskNotes/Tasks/llvm-5-criterion-drift-sweep.md` (this note — progress log)
- Read-only: `test/**` (all of it), `Dockerfile`, `CMakeLists.txt`, `utils/**`

## Blocked by

- ~~llvm-5-seq-variant-failures~~
- ~~llvm-5-matrix-multiply-verdict~~

Run first among the open tasks — it is short, and `llvm-5-port-closeout` cannot state a final
result until the last failing test's verdict is trustworthy. Must not run concurrently with any
task that runs the suite.

## Progress log

- 2026-08-14 `TBD` — Sweep: indices 291 and 292 match the golden exactly (drift +7 from 285 to 292, both `!dbg !259` → line 97 `dprintf("\n")`). Rewrote `matrix_multiply-seq.md`, updated `SUMMARY.md` and `AGENTS.md` Current state. TODO: re-run full suite.

## Handoff
- branch `agent/llvm-5-criterion-drift-sweep`
Refs: `porting/TaskNotes/Tasks/llvm-5-matrix-multiply-verdict.md`,
`porting/TestAudit/llvm-5.0.2/matrix_multiply-seq.md`, `porting/TestAudit/llvm-5.0.2/SUMMARY.md`,
`test/matrix_multiply/matrix_multiply-seq.c`, `AGENTS.md`
