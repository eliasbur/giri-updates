# AGENTS.md

## Current state

This branch (`port/llvm-8.0.0`) is porting Giri from LLVM 3.4 to LLVM/Clang 8.0.0. **Phases 1–3 are done (CMake build system, mechanical renames, DataLayout-as-pass removal). Phases 4–6 are not done** — the build compiles through `librtgiri.a` and then fails at the expected unresolved errors. See `porting/llvm-releases/8.0.0/api-breakings.yaml` for structured porting tasks.

This is the portable core. Every version branch (`port/llvm-*`) carries a copy of this file
and may append a `## Current state` section at the top with branch-specific status.

## Build

Giri is built and tested **only inside Docker** — never on the host.

```bash
docker build -t <image-name> .         # full image build (includes LLVM toolchain)
docker run -it --rm <image-name> bash  # iterate inside container without rebuild
```

Don't rebuild the Docker image for every code change — the LLVM toolchain is cached early;
iterate inside a running container with:

```bash
cmake --build build
make -C test test
```

`utils/build.sh` uses CMake 3.10-compatible syntax (`cd build && cmake ..`, no `-S`/`-B`).
Do not switch to modern CMake flags unless you also update the Dockerfile to install CMake >=3.13.

Build output is flat: `build/lib` (`libgiri.so`, `libdgutility.so`, `librtgiri.a`) and
`build/bin` (`tracer`, `prtrace`).

## Testing

```bash
make -C test test                          # full suite (from auto-tests.txt)
cd test/UnitTests/test10 && make && make test  # single test
```

## Architecture

Two LLVM passes, connected only by a binary trace file and a deterministic numbering scheme:

1. **`-trace-giri`** (`TracingNoGiri`) — instruments IR so runtime logs every BB entry, load,
   store, call, return, select to a `.trace` file.
2. **`-dgiri`** (`DynamicGiri`) — reads the trace back and computes the backwards slice from a
   criterion (`-criterion-loc=file:42` or `-criterion-inst=<id>`).

Numbering passes (`-bbnum`/`-lsnum`) assign stable numeric IDs to BBs and load/store instructions;
**ID assignment must be deterministic** across separate instrumentation and slicing runs on the same IR.

## Code layout

| Path | Purpose |
|---|---|
| `lib/Giri/TracingNoGiri.cpp` | Instrumentation pass |
| `lib/Giri/Giri.cpp` | Backward-slice computation |
| `lib/Giri/TraceFile.cpp` | (~1500 lines) Trace parser, most subtle file |
| `include/Giri/Runtime.h` | `Entry` struct (trace record format) — ABI boundary |
| `runtime/Giri/Tracing.cpp` | `librtgiri` — C++/pthreads, **no LLVM dependency** |
| `lib/Utility/BasicBlockNumbering.cpp` | BB numbering |
| `lib/Utility/LoadStoreNumbering.cpp` | Load/store numbering |
| `lib/Utility/PostDominatorFrontier.cpp` | Post-dominance frontier |
| `lib/Utility/SourceLineMapping.cpp` | Debug-info -> `file:line` mapping |

## Critical invariants (port must preserve)

1. **Numbering determinism** — `-bbnum`/`-lsnum` must assign identical IDs in both tracing and slicing runs.
2. **`Entry` struct ABI** — `Runtime.h` defines the record format; writer and reader must agree exactly.
3. **Pass registration** — test Makefiles spell out explicit `opt -load ... -passname` order.

## Code conventions

- LLVM 3.4-vintage C++ — old pass manager, `OwningPtr` in places, no range-for rewrites.
- Pre-existing filename inconsistency: `include/Utility/PostDominanceFrontier.h` vs
  `lib/Utility/PostDominatorFrontier.cpp` — not a typo.

## Version-specific docs

- **Porting guide:** `porting/AgentGuide.md` — detailed build/test/debugging commands
- **How Giri works:** `porting/HowItWorks.md` — deep dive into the tracing/slicing pipeline
- **LLVM API changes:** `porting/llvm-releases/<version>/api-breakings.yaml` — structured API deltas
- **Porting tasks:** `porting/task-notes/Tasks/` — agentic task notes for the handle-task skill