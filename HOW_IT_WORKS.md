# How Giri Works

This document explains, concretely, how Giri computes a dynamic backwards slice of a C program,
walking through the actual pipeline of tool invocations, LLVM passes, and file formats involved. It
is meant for two audiences at once:

- **Human readers** who want to understand or debug the tool.
- **LLM agents** (including future instances of Claude) doing the LLVM 3.4 → 8.0 port described in
  [`PORTING.md`](PORTING.md) — this document is the "what must keep working" reference. If you are
  porting Giri, read this *and* `PORTING.md` before changing pass code; this file tells you what
  each moving part is *for*, `PORTING.md` tells you which APIs it uses have changed.

See [`CLAUDE.md`](CLAUDE.md) for build/test commands and repo layout; this document goes deeper on
the mechanism itself.

## The core idea

Giri does not slice statically from source. It **runs the program once on a real input, records
everything that happened at runtime into a binary log (the *trace*), then walks that log backwards**
from a chosen instruction/value (the *slicing criterion*) to find every instruction whose execution
or value actually contributed to it.

This is what makes it *dynamic* slicing rather than static slicing: which branch was actually taken,
and which memory address a given pointer actually dereferenced, are resolved by observing one
concrete execution instead of reasoning conservatively about all possible executions. Static slicing
would have to over-approximate aliasing and control flow; Giri instead pays for a runtime trace and
gets exact answers for that one run.

The whole system is built as two separate LLVM passes over the same LLVM IR module, connected only
by (a) a binary trace file and (b) a deterministic instruction/basic-block numbering scheme:

1. **`TracingNoGiri`** (`-trace-giri`) — instruments IR so the compiled program logs itself at
   runtime.
2. **`DynamicGiri`** (`-dgiri`) — an offline analysis pass that reads the trace back and computes the
   slice.

## Pipeline, mapped onto the normal C toolchain

Normal C pipeline: `preprocess → compile → assemble → link → run`. Giri inserts an instrumentation
pass between "compile to IR" and "generate machine code," and adds an entirely separate offline
analysis phase that consumes the trace produced by running the instrumented binary. Every step below
corresponds directly to a rule in [`test/Makefile.common`](test/Makefile.common), which is the
authoritative source for exact flags — read that file, not just this summary, when in doubt.

```
source.c ──clang -emit-llvm──► IR ──llvm-link──► whole-program IR (program.all.bc)
                                                        │
                          ┌─────────────────────────────┴───────────────────────────┐
                          ▼ (-trace-giri, adds Record* calls)                        │ (kept pristine)
                    instrumented IR (program.trace.bc)                               │
                          │ llc + link w/ librtgiri                                  │
                          ▼                                                          │
                    self-tracing executable ──run on real input──► binary .trace log │
                                                                            │         │
                                                                            ▼         ▼
                                                                    -dgiri pass (criterion + trace + original IR)
                                                                            │
                                                                            ▼
                                                              backward slice → source line numbers
```

### 1. Compile to whole-program IR (stop before machine code)

```sh
clang -g -O0 -c -emit-llvm file.c -o file.bc     # per translation unit
llvm-link *.bc -o program.all.bc                  # whole-program module
```

Each `.c` file compiles to LLVM bitcode instead of going straight to assembly. All translation units
are then `llvm-link`ed into a single whole-program module, because both instrumentation and slicing
need to see the whole call graph — a dynamic dependency can cross function/file boundaries.

`-g` is required: debug info is how the final slice gets mapped back to source `file:line`.

### 2. Instrumentation pass (replaces "assemble", conceptually)

```sh
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum -trace-giri -trace-file=program.trace \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o program.trace.bc
```

Three passes prepare the ground before instrumentation runs:

- **`-mergereturn`** normalizes every function to a single return block, simplifying later
  control-dependence reasoning.
- **`-bbnum`** (`BasicBlockNumbering`, `include/Utility/BasicBlockNumbering.h`) and **`-lsnum`**
  (`LoadStoreNumbering`, `include/Utility/LoadStoreNumbering.h`) assign every basic block and every
  load/store instruction a **stable numeric ID**.

  **This numbering is the glue holding the whole two-phase design together.** It is deterministic on
  a given IR module: the instrumentation pass (step 2, working on the whole module) and the slicing
  pass (step 5, working on the module again, separately) must derive **identical IDs** for the same
  instructions, purely from walking the IR in the same order, because trace records only carry a
  numeric ID — not a pointer, not a name. If a port changes iteration order over basic blocks or
  instructions (e.g. because of an LLVM IR container/API change), numbering can silently diverge
  between the two passes and slices will silently become wrong without any crash. **Treat any change
  touching `BasicBlockNumbering`/`LoadStoreNumbering` as high-risk and verify against the test suite.**

Then **`-trace-giri`** runs `TracingNoGiri` (`include/Giri/Giri.h`, `lib/Giri/TracingNoGiri.cpp`), a
`BasicBlockPass` + `InstVisitor<TracingNoGiri>`. It walks every instruction and injects calls to a
runtime library:

| Injected around... | Runtime call | Purpose |
|---|---|---|
| top of every basic block | `RecordBB` / `RecordStartBB` | logs BB entry (its numeric ID) |
| `load` | `RecordLoad` | logs the concrete memory address + size read |
| `store` | `RecordStore` | logs the concrete memory address + size written |
| `select` | `RecordSelect` | logs which operand was chosen |
| `call` / special externals (`memcpy`, `memset`, string functions, ...) | `RecordCall`, `RecordExtCall`, `RecordStrLoad`/`RecordStrStore`/`RecordStrcatStore`, ... | logs call sites and memory effects of opaque library calls |
| `ret` | `RecordReturn` | logs function return, tying callee trace back to caller |
| pthread-created functions / locks | `RecordHandlerThreadID`, `RecordLock`/`RecordUnlock` | thread-id and lock bookkeeping for multi-threaded traces |

The result is a new bitcode module that behaves like the original program but also narrates its own
execution. `-remove-bbnum`/`-remove-lsnum` then strip the analysis-only pass state before the module
is written out (the numbering passes are re-derived independently at slicing time, not carried
through the IR).

### 3. Ordinary codegen + link, but against the tracing runtime

```sh
llc -O0 program.trace.bc -o program.trace.s
clang++ program.trace.s -o program.trace.exe -lrtgiri
```

This is the normal back half of a compiler pipeline (codegen, then link) — except the link pulls in
`librtgiri`, the tracing runtime (`runtime/Giri/Tracing.cpp`), which implements the `Record*`
functions the instrumentation calls. Each call appends a fixed-size `Entry` record
(`include/Giri/Runtime.h`) to a binary trace file.

**The `Entry` record format** (`include/Giri/Runtime.h`):

```c
enum class RecordType : unsigned {
  BBType = 'B',  // Basic block record
  LDType = 'L',  // Load record
  STType = 'S',  // Store record
  CLType = 'C',  // Call record
  RTType = 'R',  // Call return record
  ENType = 'E',  // End record
  PDType = 'P'   // Select (predicated) record
};

struct Entry {
  RecordType type;      // what kind of event
  unsigned   id;         // BB id or load/store id, from the numbering passes
  pthread_t  tid;        // thread id
  uintptr_t  address;    // runtime memory address (load/store); function address for BB records
  uintptr_t  length;     // access size (load/store); overloaded for other record types
  // padding to keep sizeof(Entry) a divisor of both the page size and the
  // runtime's in-memory cache size — see the warning comment in Runtime.h
  // before adding/removing fields.
};
```

**Porting note:** if a port changes this struct's layout or size (e.g. due to `pthread_t` size
differences across platforms/toolchains, or alignment changes from a newer compiler), it invalidates
the size-divisibility invariant called out in the header comment and can break the runtime's
`mmap`-based caching. Treat this struct as an ABI boundary between `runtime/Giri/Tracing.cpp` and
`lib/Giri/TraceFile.cpp` — both must agree on it exactly.

### 4. Run the instrumented binary on a real input

```sh
./program.trace.exe <real input>
```

You just run the instrumented binary like any other program. Its actual side effect is the `.trace`
file: a chronological, binary log of every basic block entered, every load/store address touched,
and every call/return, for that one concrete execution. **This is the artifact static analysis can
never produce** — it captures which branch was actually taken and which specific memory address a
given pointer dereferenced, at this specific runtime.

### 5. Slicing: a second, offline analysis pass over the same IR + the trace

```sh
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum \
    -dgiri -trace-file=program.trace -slice-file=program.slice \
    -criterion-loc=file.c:42   `# or -criterion-inst=<id>` \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o /dev/null
```

Note this reruns `-mergereturn`/`-bbnum`/`-lsnum` on the **original, non-instrumented** IR
(`program.all.bc`, not `program.trace.bc`) — determinism of the numbering passes is what makes the
resulting IDs line up with what's already sitting in the trace file from step 2/3.

`DynamicGiri` (`include/Giri/Giri.h`, `lib/Giri/Giri.cpp`) is a `ModulePass` that:

1. **Loads the trace** via `TraceFile` (`lib/Giri/TraceFile.cpp`, ~1500 lines — the largest and most
   subtle file in the codebase). It `mmap`s the trace file and implements the search operations
   needed to answer "what happened before this point in the trace," e.g. `findPreviousID` and
   `findAllStoresForLoad` (which finds the most recent dynamic store — handling partial overlaps —
   to the exact address a given dynamic load read from). See the recent commit history for an
   example of a subtle bug fix in `findAllStoresForLoad` around partial-overlap matching; changes to
   this file need care around trace indices, thread IDs, and partial-overlap load/store matching.
2. **Resolves the slicing criterion** (`-criterion-loc` source location or `-criterion-inst`
   instruction ID) to one *specific dynamic occurrence* of that instruction in the trace — represented
   as a `DynValue` (a static `Value*` paired with a trace-occurrence index), since the same static
   instruction can execute many times (e.g. inside a loop) and each occurrence can have a different
   dynamic slice.
3. **Runs a worklist-based backward traversal** (`findSlice`, using `std::deque<DynValue*>` as the
   worklist and a `std::unordered_set<DynValue>`/`std::set<DynValue*>` as processed-set/data-flow-graph)
   from that occurrence, following two kinds of dependence edges:
   - **Data dependence.** For register/SSA operands, this is just the static LLVM def-use chain — no
     trace needed. For a `load`, the def-use chain alone isn't enough: which store it reads from
     depends on runtime aliasing, so `TraceFile` is asked which dynamic store most recently wrote
     that exact runtime address before this dynamic load. **This is the one step that fundamentally
     requires the runtime trace** — everything else could in principle be done statically (with much
     coarser precision).
   - **Control dependence.** Using `PostDominatorTree` (LLVM built-in) and `PostDominanceFrontier`
     (`include/Utility/PostDominanceFrontier.h`, a Giri-maintained utility pass — see `PORTING.md`
     for the open question about its build-dependency chain on newer LLVM), `findExecForcers`
     determines which branch decisions were necessary to force execution of each basic block already
     in the slice, pulling those branch instructions (and transitively their own dependencies) in
     too. Note the subtlety documented in `Giri.h`: a block that post-dominates the entry block is
     unconditionally executed on function entry, which is different from being control-dependent on
     itself — `findExecForcers`'s return value distinguishes "will execute at least once" from
     "conditionally executes."
4. Accumulates a static `std::set<Value*> Slice` and a dynamic
   `std::unordered_set<DynValue> DynSlice` / `std::set<DynValue*> DataFlowGraph`, the latter being
   effectively the reconstructed per-execution data/control-flow graph feeding the criterion.

### 6. Mapping back to source lines

`SourceLineMappingPass` (`include/Utility/SourceLineMapping.h`,
`lib/Utility/SourceLineMapping.cpp`) uses the debug info attached by the original `-g` compile
(`locateSrcInfo`) to translate each sliced LLVM instruction back to a `file:line`. `DynamicGiri`
writes these out to the `.slice` file (lines matching `Source ... <line>`), and
`Makefile.common`'s `%.slice.loc` rule reduces that with `sed`/`awk`/`sort`/`uniq` to a sorted,
de-duplicated list of source line numbers — the final human-readable answer: *these are the lines of
your C source that actually influenced the value at your slicing criterion, for this specific run.*
That file is what `make test` diffs against the checked-in `ans-*.txt`/`criterion-*.txt` answer keys.

## Key invariants a port must preserve

These are the load-bearing assumptions that make the two-pass, trace-mediated design work at all.
If a port changes any of these, dynamic slices can become *silently* wrong (no crash, no test
failure necessarily, just incorrect slices) rather than failing loudly — so they deserve explicit
verification, not just "it compiles":

1. **Numbering determinism across passes.** `-bbnum`/`-lsnum` must assign the same IDs to the same
   basic blocks/load-store instructions whether run during instrumentation (on `program.trace.bc`'s
   ancestor) or during slicing (on `program.all.bc` directly), given the two runs start from
   equivalent IR. This depends on iterating IR containers (functions, basic blocks, instructions) in
   a stable, reproducible order.
2. **`Entry` struct ABI stability** between `runtime/Giri/Tracing.cpp` (writer, compiled into the
   traced program) and `lib/Giri/TraceFile.cpp` (reader, compiled into the `opt` plugin) — same
   struct layout, same size-divides-page-size invariant.
3. **Debug info availability and shape.** `SourceLineMapping` depends on `-g` debug info attached to
   instructions; LLVM's debug info representation changed across versions (see `PORTING.md`'s note
   on debug-info API changes) — the *mapping instruction → file:line* capability must survive the
   port even if the underlying metadata APIs used to get there change.
4. **Pass registration and ordering.** The pipeline hard-codes pass ordering via explicit `opt -load
   ... -passname` flags in the Makefiles (not an automatic pass pipeline), and `getAnalysisUsage`
   declarations (`AU.addRequired<...>`, `addRequiredTransitive<...>`, `addPreserved<...>`) encode
   the same ordering/dependency constraints inside the passes themselves. Both must stay consistent
   with each other after a port.

## Where to look for what

| Question | File |
|---|---|
| How is a BB/load/store numbered? | `include/Utility/BasicBlockNumbering.h`, `include/Utility/LoadStoreNumbering.h` |
| What gets instrumented, and how? | `include/Giri/Giri.h` (`TracingNoGiri`), `lib/Giri/TracingNoGiri.cpp` |
| What does the runtime actually write? | `runtime/Giri/Tracing.cpp`, `include/Giri/Runtime.h` (`Entry`) |
| How is the trace file parsed/searched? | `lib/Giri/TraceFile.cpp` (largest, most subtle file — memory-dependence search lives here) |
| How is the backward slice actually computed? | `include/Giri/Giri.h` (`DynamicGiri`), `lib/Giri/Giri.cpp` |
| How does control dependence get computed? | `include/Utility/PostDominanceFrontier.h` + LLVM's `PostDominatorTree` |
| How does a slice get back to `file:line`? | `include/Utility/SourceLineMapping.h`, `lib/Utility/SourceLineMapping.cpp` |
| Exact build/trace/slice command lines | `test/Makefile.common` |
| Dump a trace file for debugging | `tools/PrintTrace/` (`prtrace` binary) |
| LLVM 3.4 → 8.0 API breaks already identified | `PORTING.md` |
