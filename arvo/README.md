# Running Giri on ARVO containers

An ARVO container reproduces one OSS-Fuzz bug: `arvo compile` rebuilds the
project from `/src` via `$SRC/build.sh`, and `arvo` runs `/out/<fuzzer> /tmp/poc`
to reproduce the crash under a sanitizer.  Giri needs something quite different
— one whole-program LLVM module, instrumented by its own passes, executed
exactly once — so the job is to make ARVO's own build produce that module as a
side effect.

Everything here hangs off env hooks ARVO/OSS-Fuzz already provides (`$CC`,
`$CXX`, `$FUZZING_ENGINE`, `$SANITIZER_FLAGS_*`).  **`$SRC/build.sh` is never
edited.**

Validated end to end on `arvo-vscode:42473917-vul` (ffmpeg SAMI decoder,
heap-buffer-overflow at `libavcodec/htmlsubtitles.c:174`).

## Why each piece exists

| Problem | Solution |
|---|---|
| Giri slices one module; the project builds hundreds of objects into static archives | `giri-cc` compiles every TU twice — the native object as usual, plus an `-O0 -g` bitcode copy whose path is stamped into an `.llvm_bc` section of the object.  The section survives `ar` and the final link, so the linked binary carries the exact list of TUs that went into it (the wllvm trick).  `giri-extract-bc` reads it back and `llvm-link`s them. |
| libFuzzer's `main` is native C++ and loops over inputs | `compile_giri` is a fake "fuzzing engine": it installs a 20-line `main()` replaying a single input file.  `compile` sources `compile_${FUZZING_ENGINE}`, so `FUZZING_ENGINE=giri` swaps it in build-wide.  The driver goes through `giri-cc`, so `main` is in the bitcode too — which is what the runtime's ctor/`atexit` pair needs. |
| ASan's handler `_exit()`s, truncating the trace; its instrumentation would sit inside the sliced bitcode | `SANITIZER_FLAGS_address=""` and `COVERAGE_FLAGS=""`.  Nothing is lost: the unsanitized run still executes the offending instruction, and ASan's report is what gave you the criterion in the first place. |
| Some projects hardcode sanitizers in their own build system | ffmpeg's `--enable-ossfuzz` puts `-fsanitize=address,undefined` on the link line where env vars cannot reach.  `giri-instrument` strips sanitizer flags when it relinks. |
| The traced object must relink against the same libraries | `giri-cc` records every link command it sees; `giri-instrument` replays it with the instrumented object substituted for the objects it absorbed. |
| Giri's passes need LLVM's libstdc++; ARVO links `-stdlib=libc++` | Build the passes with a cleared env, then rebuild **only the runtime** (no LLVM dependency) against the target's C++ library via `giri-build-runtime`. |

## Recipe

```bash
# --- once per container -----------------------------------------------------
docker cp <giri-source>/. <container>:/giri-src
docker cp arvo/. <container>:/giri/          # these scripts, into /giri/bin
docker exec <container> bash -lc '
  cd /giri-src && for p in /giri/000*.patch; do patch -p1 < "$p"; done
  mkdir -p build && cd build &&
  env -u CFLAGS -u CXXFLAGS -u CXXFLAGS_EXTRA -u CC -u CXX \
      cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)'

# --- build the target with bitcode capture ----------------------------------
source /giri/env.sh
giri-build-runtime               # runtime against the target's C++ stdlib
compile                          # ARVO's own compile, NOT `arvo compile`

# --- instrument, trace, slice -----------------------------------------------
giri-instrument $OUT/<fuzzer>
/giri/work/<fuzzer>/<fuzzer>.trace.exe /tmp/poc
giri-criterion /giri/work/<fuzzer>/<fuzzer>.all.bc 'libavcodec/htmlsubtitles.c:174'
giri-slice     /giri/work/<fuzzer>/<fuzzer> ff_htmlmarkup_to_ass:571
```

Run `compile` directly, not `arvo compile`: `/bin/arvo` re-exports
`FUZZING_ENGINE=libfuzzer`, `SANITIZER=address` and friends at the top, undoing
`env.sh`.

`giri-instrument` looks up the recorded link line by the binary's basename.
Build systems often rename the binary afterwards (ffmpeg's `build.sh` moves
`tools/target_dec_sami_fuzzer` to `$OUT/ffmpeg_AV_CODEC_ID_SAMI_fuzzer`), in
which case it lists the candidates — pass the right one as `GIRI_LINKCMD`.

## Choosing the criterion

The natural criterion for an ARVO bug is the frame ASan blames: `#1` in the
report, the first frame inside the project rather than the interceptor.

**Prefer `-criterion-inst` (`function:index`) over `-criterion-loc`
(`file:line`).**  `-criterion-loc` keeps the *last* instruction in module order
carrying that line, which for a loop header is the latch inside the loop body.
For `htmlsubtitles.c:174` —

```c
while (dst->len >= 2 && !strncmp(&dst->str[dst->len - 2], "\\N", 2))
    dst->len -= 2;                       /* line 175 */
```

— that is `br label %488` in `while.body`, a block that only executes if the
buffer really does end in `\N`.  It did not execute for this PoC, so
`getLastDynValue()` found nothing in the trace and returned index 0, which the
slicer then walked off the end of (patch 0003 turns that into a clean miss, but
the criterion is still the wrong instruction).

`giri-criterion` prints every candidate with the index `-criterion-inst` wants,
numbered by Giri's own `-srcline-mapping` pass so the two always agree:

```
ff_htmlmarkup_to_ass:570    %502 = getelementptr inbounds i8, i8* %496, i64 %501
ff_htmlmarkup_to_ass:571    %503 = call i32 @strncmp(i8* %502, ...)   <- ASan frame #1
ff_htmlmarkup_to_ass:582    br label %488, !llvm.loop        <- what -criterion-loc picks
```

## Giri fixes required to get this far

Four defects only show up at real-program scale; all four are in upstream Giri,
not in the LLVM 5 port.  Patches are in this directory and apply cleanly to
`port/llvm-5.0.2`.

**0001 — the tracing pass is quadratic in function size.**
`instrumentLock`/`instrumentUnlock` pretty-print the whole instruction to a
string and create a fresh `GlobalVariable` for it, on every instrumented load,
store, call and basic block.  `Instruction::print()` builds a `SlotTracker` over
the enclosing function each time.  The strings are only ever passed to a
`DEBUG()` in the runtime that compiles to `do {} while (false)`.  Measured on
subsets of the ffmpeg module:

| linked TUs | IR lines | before | after |
|---|---|---|---|
| 5 | 6 042 | 11 s | 0 s |
| 20 | ~25 000 | > 10 min | 1 s |
| 91 (whole target) | — | hours | **18 s** for the whole bitcode→object stage |

**0002 — use-after-destruction of the runtime's static maps.**
`finish()` is registered with `atexit()` from `recordInit()`, which runs from the
instrumented module's global constructor.  The instrumented objects link ahead
of `librtgiri.a` (they define `main`), so that ctor runs *before* `Tracing.cpp`'s
statics are constructed — which sequences the atexit handler before their
destructors.  `closeCacheFile()` then walks a destroyed `std::unordered_map`.
Every traced ffmpeg run died with `[GIRI] Abnormal termination, signal number 11`
in `closeCacheFile`, confirmed under gdb.  Allocating the two maps on the heap
and never freeing them makes the flush order-independent.

**0004 — printing the slice is quadratic in module size.**
`DynValue::print()` calls the one-argument `llvm::Value::print()`, which
constructs an `AssemblyWriter` whose `init()` runs `TypeFinder::run()` over the
whole module — so a slice of N values costs N whole-module type scans.  Sharing
a `ModuleSlotTracker` does **not** fix it (measured: 91 → 86 values/sec): the
tracker shares slot numbering, not the type table, and LLVM exposes no way to
share the latter.  `-slice-terse` instead omits the IR text, printing opcode,
function, load/store ID and trace index — everything the `.slice.loc` digest
consumes.  The criterion itself is still printed in full IR.  The same slice
went from **>50 min, unfinished (42 %)** to **23 s, complete**, with a
byte-identical source-line digest (verified against the `test2` golden).
`giri-slice` passes the flag by default; `GIRI_SLICE_TERSE=0` restores the
original format.

**0003 — unsigned wrap in `TraceFile::findPreviousID`.**
The `set<unsigned>` overload is written `do { ... --index; } while (index != 0)`,
so `start_index == 0` decrements to `ULONG_MAX` and indexes far outside the
mapped trace.  `getLastDynValue()` legitimately returns index 0 when a value
never appears in the trace.  The sibling overload twenty lines above is already
written the safe way; this makes them match (and lets entry 0 be examined).

## Measured on the sample container

| | |
|---|---|
| Toolchain in container | clang/LLVM **5.0.0**, full `/usr/local` dev tree |
| TUs captured during the ARVO build | 2 950, **0** bitcode failures |
| TUs actually linked into the SAMI fuzzer | 91 (5.0 MB `.all.bc`) |
| Instrument + codegen + relink | 18 s |
| Traced run on `/tmp/poc` | 0.1 s, exit 0 |
| Trace | 28 MB, 899 276 entries (258 526 BB, 420 184 load, 140 162 store, 40 199 call/return, 5 select, 1 end) |
| Slice from `ff_htmlmarkup_to_ass:571` | reaches `standalone_driver.c:24-30` — the `fread` of the PoC bytes |

## Limits to expect

* **Only what is in the bitcode is traced.**  Assembly-only archive members,
  system libraries, and any TU whose `-O0` bitcode compile failed (logged to
  `$GIRI_BC_DIR/failed.log`) link natively and are invisible.  Their stores
  become *lost loads* at slicing time.  For ffmpeg the gap is exactly the
  yasm-built `libavcodec/x86/*.o` (645 bitcode vs 861 objects in libavcodec).
* **Giri models a fixed set of libc functions and no others**
  (`lib/Giri/TracingNoGiri.cpp:218`): `llvm.memset/memcpy/memmove`, `strcpy`,
  `strcat`, `strlen`, `calloc`, `sprintf`, `sscanf`, `fscanf`.  A direct call to
  `memcpy` (rather than the intrinsic) and — relevant here — `realloc`, which is
  how `av_bprint` grows the very buffer this bug overflows, are **not** modelled.
  Watch `Number of Dynamic Loads Lost` in `-stats`.
* **Slicing is the slow stage, not tracing** — but with patch 0004 the whole
  slice takes 23 s.  What remains is `findPreviousID` linearly rescanning the
  trace per query, so cost is roughly queries × trace length, with no index and
  no memoisation.  That term grows with *trace* length, not module size, so a
  long-running PoC is the thing to watch; narrow the module with
  `GIRI_BC_INCLUDE` if it bites.
* **`-stats` is a no-op on this container.**  Its LLVM is a Release build with
  assertions off, so `NDEBUG` compiles out `STATISTIC` — including `Number of
  Dynamic Loads Lost`.  Use the `getSourcesForCall failed to find` lines on
  stderr as the proxy, or build against an assertions-enabled LLVM.
* **The trace runtime mmaps 10 % of physical RAM** as its entry buffer and
  extends the trace file to match (`runtime/Giri/Tracing.cpp:123`) — on a 1 TB
  host that is a 100 GB sparse file, truncated to the real length at exit.
* **A killed traced process leaves an unusable trace.**  Without `finish()` the
  file keeps its full sparse size and has no `ENType` terminator, and Giri's
  trace scans have no bound check — they segfault.  Re-run the binary to
  completion rather than debugging the slicer against such a file.
* **No sanitizer means no crash.**  The traced run executes the faulting
  instruction and carries on.  That is what you want for slicing, but the traced
  binary is not a reproducer.

## Other ARVO containers

The sample container ships clang 5.0.0, which is why Giri builds inside it
directly.  ARVO pins whatever toolchain OSS-Fuzz used at the time, so this will
not always hold — the neighbouring `arvo-42471526-vul` has clang 8.0.0.  In
order of preference:

1. **Container LLVM matches a Giri port** (`llvm-config --version`) → build Giri
   in the container as above.
2. **Container LLVM is newer** → keep the native build on the container's clang
   and set `GIRI_BC_CC`/`GIRI_BC_CXX` to a matching-version clang installed
   alongside (release tarballs unpack fine; the containers have working
   network).  `giri-cc` uses the two compilers independently, so the project
   still builds with the toolchain it expects while the bitcode stays readable
   by Giri's passes.  Sources that only compile with the newer clang drop out
   and are logged.
3. **Neither** → port Giri to that LLVM version, which is what the `port/llvm-*`
   branches are for.

## Files

| File | Role |
|---|---|
| `giri-cc` | compiler wrapper; install as `giri-clang` / `giri-clang++` |
| `compile_giri` | OSS-Fuzz "engine" installing the single-shot driver |
| `standalone_driver.c` | the `main()` that replaces libFuzzer's |
| `giri-env.sh` | env that redirects an ARVO build through the above |
| `giri-build-runtime` | rebuild `librtgiri.a` against the target's C++ stdlib |
| `giri-extract-bc` | `.llvm_bc` section → one linked `.bc` |
| `giri-instrument` | bitcode → `-trace-giri` → codegen → relink |
| `giri-criterion` | `file:line` → `function:index` candidates |
| `giri-slice` | run `-dgiri` with a criterion against the trace |
| `000{1,2,3,4}-*.patch` | the four Giri fixes described above |
| `RESULTS-sample-container.md` | the measured slice for the sample bug |
| `AUTOMATION.md` | the pipeline as automatable stages, and what still needs a human |
