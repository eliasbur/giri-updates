# Automating Giri on ARVO containers

Why each step exists, and the limits of the approach, are in [README.md](README.md);
the measured result for the sample bug is in
[RESULTS-sample-container.md](RESULTS-sample-container.md). This document covers
the pipeline as a driver runs it, and what still needs a human.

[`giri-arvo`](giri-arvo) runs the whole thing:

```
giri-arvo <container> [--poc /tmp/poc] [--criterion file:line] [--out DIR]
```

It targets **the LLVM 5 port only** — that is what stage 0 checks.

## The pipeline as stages

Every stage is a single command, and every stage has a gate. "Auto" means it
needs no human judgement given a container and a PoC.

| # | Stage | Command | Auto | Fails when |
|---|-------|---------|------|-----------|
| 0 | Toolchain gate | `llvm-config --version` | yes | container LLVM is not 5.x — see [Per-container gating](#per-container-gating) |
| 1 | Target + report | `/out/<fuzzer> <poc>` | yes | the container's own binary does not reproduce |
| 2 | Install Giri | `git archive` into `/giri-src`, wrappers into `/giri/bin` | yes | — |
| 3 | Build Giri | `cmake … && make` with `env -u CFLAGS -u CXXFLAGS -u CXXFLAGS_EXTRA -u CC -u CXX` | yes | ARVO's `-stdlib=libc++` leaks in; LLVM's static libs are libstdc++ |
| 3 | Runtime rebuild | [`giri-build-runtime`](giri-build-runtime) | yes | target's `CXXFLAGS` unavailable |
| 4 | Build target | `source `[`giri-env.sh`](giri-env.sh)`; compile` | yes | **`build.sh` already ran once in this container** — see below |
| 5 | Whole-program bitcode | [`giri-extract-bc`](giri-extract-bc) (called by stage 5) | yes | no `.llvm_bc` section — target not built through `giri-cc` |
| 5 | Instrument + relink | [`giri-instrument`](giri-instrument) `$OUT/<fuzzer>` | yes | — |
| 6 | Trace | [`giri-trace`](giri-trace) `<base> <poc>` | yes | crash mid-run leaves an unusable trace, and it says so |
| 7 | Criterion | [`giri-criterion`](giri-criterion) `<all.bc> --asan <report>` | yes | no project frame in the report |
| 8 | Slice | [`giri-slice`](giri-slice) `<base> <function:index>` | yes | every candidate criterion never executed |
| 9 | Collect | — | yes | — |

Stage 4 is the only long one (~40 min for ffmpeg with all dependencies). Stages
5, 6 and 8 together took 23 s + 0.1 s + 25 s on the sample target.

## Environment contract

[`giri-env.sh`](giri-env.sh) is the single source of truth and should be sourced
rather than reproduced. It sets the OSS-Fuzz hooks (`CC`, `CXX`,
`FUZZING_ENGINE=giri`, `SANITIZER_FLAGS_*=""`, `COVERAGE_FLAGS=""`, `OUT`) plus
`GIRI_ROOT` and `GIRI_BC_DIR`.

Variables a driver may additionally want to set:

| Variable | Read by | Use |
|---|---|---|
| `GIRI_BC_CC` / `GIRI_BC_CXX` | `giri-cc` | bitcode compiler ≠ build compiler (newer-LLVM containers) |
| `GIRI_BC_INCLUDE` / `GIRI_BC_EXCLUDE` | `giri-extract-bc` | narrow the module; the main lever on trace size and slice time |
| `GIRI_BC_OPT` | `giri-cc` | bitcode optimisation level (default `-O0`) |
| `GIRI_LINKCMD` | `giri-instrument` | recorded link line, when the `.giri_link` section is absent |
| `GIRI_TRACE_TIMEOUT` | `giri-trace` | seconds before the traced run is killed (default 600) |
| `GIRI_BUILD`, `GIRI_RT_DIR` | several | Giri build tree; runtime archive |
| `GIRI_SLICE_TERSE=0` | `giri-slice` | restore full-IR slice output |

Run `compile` directly, never `arvo compile` — `/bin/arvo` re-exports
`FUZZING_ENGINE=libfuzzer` and `SANITIZER=address`, undoing the env.

## What the driver does about the five blockers

The five items this document used to list as blocking unattended operation, and
where each one stands.

### 1. Toolchain gate (stage 0) — branched, not removed

Still the one genuinely per-container decision, but the driver takes it: it
reads `llvm-config --version`, proceeds on 5.x, and otherwise refuses with the
two alternatives spelled out. See [Per-container gating](#per-container-gating).

### 2. Link-line lookup after a rename (stage 5) — closed

`giri-cc` stamps the recorded link line's path into the link output as a
`.giri_link` section, exactly the way `.llvm_bc` already carries bitcode paths
one level down. A section travels inside the file, so it survives every rename
and move the build does afterwards — which is precisely when the old basename
lookup failed. `giri-instrument` resolves the link line in four steps and says
which one it used: `GIRI_LINKCMD`, the section, a basename alias, then the sole
recorded link line.

Records are keyed by a digest of (directory, compiler, argv) rather than by
basename, because two links can produce the same basename in different
directories and the second overwrote the first one's record.

### 3. No wrapper around the traced run (stage 6) — closed

[`giri-trace`](giri-trace) runs the binary once and applies three gates: stderr
carries no `[GIRI] Abnormal termination`; the trace size is a multiple of
`sizeof(Entry)` = 32; and the last 32-byte record has type `0x45` (`'E'`,
`ENType`). It deletes any stale trace first, so a failed run cannot leave an
earlier good trace behind for the slicer to answer the wrong question from, and
it exits 2 for an abnormal run versus 3 for an unusable trace so the driver can
tell them apart.

### 4. Criterion selection (stage 7) — closed, with a caveat worth reading

The sanitizer report carries more than the `file:line` the old recipe used.
[`giri-criterion`](giri-criterion) `--asan` reads three signals, in decreasing
order of how much they pin the instruction down:

* **frame #0, when it is an interceptor, names the libc function the faulting
  instruction calls.** `__interceptor_strncmp` means the criterion is a
  `call @strncmp`, which is normally unique on the line.
* **the first frame inside the project gives file, line, function and column** —
  `htmlsubtitles.c:174:30` — and columns survive into the bitcode as
  `!DILocation(column: N)`.
* **opcode class breaks the rest**, memory operations ahead of control flow,
  because a sanitizer only ever blames a memory operation.

That is a ranking, not a proof. What settles it is running the slice: Giri now
reports

```
[GIRI] criterion never executed: <spec> (<file:line>).  Its basic block does not
appear in the trace, so the slice below is empty for want of an anchor.
```

when the criterion's basic block is absent from the trace. `getLastDynValue()`
returned index 0 both for "found at entry 0" and for "not found", so this case
used to be visible only as an unexpectedly small slice — a test that guesses in
both directions. `giri-slice` turns the marker into exit 4 and the driver walks
to the next candidate.

### 5. Target selection when a build produces many fuzzers — default is the rule, not the exception

`$SRC/build.sh` is not edited. ffmpeg's loops over ~400 codecs, each a full
link, and building all of them to slice one is wasteful — so `--build-sh-sed`
is the opt-out, and it restores the file whatever happens:

```
giri-arvo arvo-42473917-vul \
  --build-sh-sed 's/for c in $CONDITIONALS/for c in ${GIRI_CODECS:-$CONDITIONALS}/' \
  --env GIRI_CODECS=SAMI
```

A driver that can afford the full build needs neither flag.

## The blocker this document did not have: `compile` runs once

**ARVO's `build.sh` is not idempotent.** 42473917's sixth line is

```
bzip2 -f -d alsa-lib-*
```

which *consumes* the archive. A second `compile` in the same container finds a
directory where it expects a `.bz2`, and dies before it reaches ffmpeg. Other
containers will have their own versions of this; it is a property of the
project's build script, not of ARVO.

Two consequences for a driver:

* **Never delete the previous generation.** `giri-arvo` renames `/giri/{bc,out,work}`
  to `/giri/prev-<timestamp>` before building. A rename is free and is one
  command to undo; a delete followed by a build that cannot re-run has destroyed
  a result nothing can reproduce. This was learned the expensive way.
* **A container is good for one build.** To re-run, restore what the build
  consumed from the image:

  ```
  cid=$(docker create <image>)
  docker cp "$cid:/src/<archive>" - | docker cp - <container>:/src/
  docker rm "$cid"
  ```

  or start a fresh container from the image, which is cheaper to reason about.

## Verification gates

The driver fails loudly at each of these rather than proceeding.

| After stage | Check | Signal |
|---|---|---|
| 3 | Giri passes load | `opt -load …/libgiri.so -dgiri --help` exits 0 |
| 4 | bitcode coverage | `$GIRI_BC_DIR/failed.log` absent or short; compare `find … -name '.*.o.bc'` against `-name '*.o'` |
| 4 | target captured | `giri-extract-bc` reports a plausible module count (91 for the sample; 0 means the target was not built through `giri-cc`) |
| 5 | relink succeeded | `.trace.exe` exists and is executable |
| 6 | clean run | stderr has no `[GIRI] Abnormal termination` |
| 6 | usable trace | trace size is a multiple of 32 and the **last** 32-byte record has type `E` (`0x45`) |
| 8 | criterion executed | no `[GIRI] criterion never executed` — `giri-slice` exit 4 |

The trace check matters most: it is the difference between a slicer segfault and
a clear diagnosis. On the sample container a good trace is 899,276 records with
exactly one `ENType`, at the last index.

## Per-container gating

ARVO pins whatever toolchain OSS-Fuzz used at the time, so this is a branch, not
a constant — the sample container has clang 5.0.0, its neighbour
`arvo-42471526-vul` has 8.0.0.

```
llvm-config --version
  ├── 5.x                           -> build Giri in-container (stage 3)
  ├── newer than any port           -> keep the native build on the container's
  │                                    clang, set GIRI_BC_CC/GIRI_BC_CXX to a
  │                                    matching clang installed alongside;
  │                                    TUs that need the newer clang drop out
  │                                    and are logged to $GIRI_BC_DIR/failed.log
  └── otherwise                     -> unsupported; port Giri to that version
```

The containers have working network, so installing a second toolchain from the
LLVM release tarballs is viable.
