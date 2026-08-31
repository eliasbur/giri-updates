# Automating Giri on ARVO containers

Why each step exists, and the limits of the approach, are in [README.md](README.md);
the measured result for the sample bug is in
[RESULTS-sample-container.md](RESULTS-sample-container.md). This document covers
only what a driver script needs in order to run the pipeline unattended, and
what currently stops it.

## The pipeline as stages

Every stage is a single command. "Auto" means it needs no human judgement given
a container and a PoC.

| # | Stage | Command | Auto | Fails when |
|---|-------|---------|------|-----------|
| 0 | Toolchain gate | `llvm-config --version` | yes | container LLVM has no matching Giri port — see [Per-container gating](#per-container-gating) |
| 1 | Build Giri | `cmake … && make` with `env -u CFLAGS -u CXXFLAGS -u CXXFLAGS_EXTRA -u CC -u CXX` | yes | ARVO's `-stdlib=libc++` leaks in; LLVM's static libs are libstdc++ |
| 2 | Apply patches | `for p in 000*.patch; do patch -p1 <$p; done` | yes | port branch has diverged from the four patches |
| 3 | Runtime rebuild | [`giri-build-runtime`](giri-build-runtime) | yes | target's `CXXFLAGS` unavailable |
| 4 | Build target | `source `[`giri-env.sh`](giri-env.sh)`; compile` | yes | project build breaks under the wrappers |
| 5 | Whole-program bitcode | [`giri-extract-bc`](giri-extract-bc) (called by stage 6) | yes | no `.llvm_bc` section — target not built through `giri-cc` |
| 6 | Instrument + relink | [`giri-instrument`](giri-instrument) `$OUT/<fuzzer>` | **partly** | binary renamed after linking → needs `GIRI_LINKCMD` |
| 7 | Trace | `<work>/<fuzzer>.trace.exe /tmp/poc` | **no wrapper exists** | crash mid-run leaves an unusable trace |
| 8 | Criterion | [`giri-criterion`](giri-criterion) `<all.bc> <file:line>` | **no** | prints candidates; choosing one is a judgement call |
| 9 | Slice | [`giri-slice`](giri-slice) `<base> <function:index>` | yes | criterion never executed → near-empty slice |

Stage 4 is the only long one (~40 min for ffmpeg with all dependencies). Stages
6, 7 and 9 together took 23 s + 0.1 s + 25 s on the sample target.

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
| `GIRI_LINKCMD` | `giri-instrument` | recorded link line, when the basename lookup fails |
| `GIRI_BUILD`, `GIRI_RT_DIR` | several | Giri build tree; runtime archive |
| `GIRI_SLICE_TERSE=0` | `giri-slice` | restore full-IR slice output (see patch 0004) |

Run `compile` directly, never `arvo compile` — `/bin/arvo` re-exports
`FUZZING_ENGINE=libfuzzer` and `SANITIZER=address`, undoing the env.

## What blocks unattended automation

Five items, in the order they bite.

### 1. Toolchain gate (stage 0)

The only genuinely per-container decision. Resolve before anything else; see
[Per-container gating](#per-container-gating).

### 2. Link-line lookup after a rename (stage 6)

`giri-cc` records link lines keyed by the output basename. Build systems rename
the binary afterwards — ffmpeg's `build.sh` moves `tools/target_dec_sami_fuzzer`
to `$OUT/ffmpeg_AV_CODEC_ID_SAMI_fuzzer` — so the lookup misses. `giri-instrument`
falls back to the sole recorded link line when there is exactly one, and
otherwise lists candidates for `GIRI_LINKCMD`; with 160 recorded links on the
sample container, that fallback does not fire.

**Fix:** stamp the link-line path into the binary the same way `giri-cc` already
stamps bitcode paths — `objcopy --add-section .giri_link=<pathfile>` on the link
output — and have `giri-instrument` read it back. The section survives renames
and moves because it is inside the file. This is *not implemented*; the
`.llvm_bc` mechanism it copies is proven, so it is a small change to the link
branch of `giri-cc` plus the lookup in `giri-instrument`.

### 3. No wrapper around the traced run (stage 7)

Nothing runs the traced binary or checks its output. A driver must do it and
apply the gates in [Verification gates](#verification-gates) — in particular, a
process killed mid-run leaves a trace with no `ENType` terminator, and Giri's
trace scans have no bound check, so the slicer segfaults on such a file.

**Fix:** a `giri-trace` script that runs the binary, greps stderr for
`[GIRI] Abnormal termination`, and verifies the last trace record is `ENType`.

### 4. Criterion selection (stage 8)

The one step needing judgement. The input is the ASan report; the useful frame is
`#1` — the first frame inside the project rather than the sanitizer interceptor.
That gives `file:line`, which `giri-criterion` turns into `function:index`
candidates. Choosing among them is not automatic: for
`libavcodec/htmlsubtitles.c:174` there were 20 candidates, and the one
`-criterion-loc` would have picked was the loop latch, in a block that never
executed.

**Toward automation:** parse frame `#1` for `file:line`; prefer the candidate
whose opcode is the memory operation ASan blamed (`call`/`load`/`store`) over
`br`/`phi`; then confirm it executed. There is no clean signal for the last
check — with patch 0003 a criterion that never executed yields a near-empty
slice rather than a crash, so the only current test is "slice is suspiciously
small, try the next candidate". A `getLastDynValue()` that reported the miss
explicitly would make this a proper gate.

### 5. Target selection when a build produces many fuzzers

ffmpeg's `build.sh` loops over ~400 codecs, each a full link. Building all of
them to slice one is wasteful. The sample run patched the loop to honour an
environment override (`for c in ${GIRI_CODECS:-$CONDITIONALS}`), which *is* an
edit to `$SRC/build.sh` — the one place the no-edit rule was broken, and it was
done for time, not necessity. A driver that can afford the full build needs no
such edit.

## Verification gates

An unattended driver should fail loudly at each of these rather than proceed.

| After stage | Check | Signal |
|---|---|---|
| 1 | Giri passes load | `opt -load …/libgiri.so -dgiri --help` exits 0 |
| 4 | bitcode coverage | `$GIRI_BC_DIR/failed.log` absent or short; compare `find … -name '.*.o.bc'` against `-name '*.o'` |
| 5 | target captured | `giri-extract-bc` reports a plausible module count (91 for the sample; 0 means the target was not built through `giri-cc`) |
| 6 | relink succeeded | `.trace.exe` exists and is executable |
| 7 | clean run | stderr has no `[GIRI] Abnormal termination` |
| 7 | usable trace | trace size is a multiple of 32 and the **last** 32-byte record has type `E` (`0x45`) |
| 9 | meaningful slice | `.slice.loc` non-trivial; a handful of lines usually means the criterion never executed |

The trace check matters most: it is the difference between a slicer segfault and
a clear diagnosis. On the sample container a good trace is 899,276 records with
exactly one `ENType`, at the last index.

## Per-container gating

ARVO pins whatever toolchain OSS-Fuzz used at the time, so this is a branch, not
a constant — the sample container has clang 5.0.0, its neighbour
`arvo-42471526-vul` has 8.0.0.

```
llvm-config --version
  ├── matches a port/llvm-* branch  -> build Giri in-container (stage 1)
  ├── newer than any port           -> keep the native build on the container's
  │                                    clang, set GIRI_BC_CC/GIRI_BC_CXX to a
  │                                    matching clang installed alongside;
  │                                    TUs that need the newer clang drop out
  │                                    and are logged to $GIRI_BC_DIR/failed.log
  └── otherwise                     -> unsupported; port Giri to that version
```

The containers have working network, so installing a second toolchain from the
LLVM release tarballs is viable.

## Driver shape

```
giri-arvo <container> [--poc /tmp/poc] [--criterion file:line]

  probe toolchain              -> gate, or select GIRI_BC_CC
  copy giri-src + arvo/ in
  apply arvo/000*.patch
  build Giri; giri-build-runtime
  source giri-env.sh; compile                       # long
  giri-instrument $OUT/<fuzzer>                     # GIRI_LINKCMD if needed
  run <fuzzer>.trace.exe $POC                       # gate on the two trace checks
  criterion from ASan report, or --criterion        # giri-criterion, then choose
  giri-slice <base> <function:index>
  emit .slice.loc + per-file digest
```

Two of these steps are not yet scripted — the traced run (item 3) and criterion
choice (item 4) — and one needs the `.giri_link` section before it is reliable
(item 2). Everything else runs unattended today.
