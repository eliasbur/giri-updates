# How Giri Works

This document explains, concretely, how Giri computes a dynamic backwards slice of a C program,
walking through the actual pipeline of tool invocations, LLVM passes, and file formats involved.

## The core idea

Giri does not slice statically from source. It **runs the program once on a real input, records
everything that happened at runtime into a binary log (the *trace*), then walks that log backwards**
from a chosen instruction/value (the *slicing criterion*) to find every instruction whose execution
or value actually contributed to it.

The whole system is built as two separate LLVM passes over the same LLVM IR module, connected only
by (a) a binary trace file and (b) a deterministic instruction/basic-block numbering scheme:

1. **`TracingNoGiri`** (`-trace-giri`) — instruments IR so the compiled program logs itself at runtime.
2. **`DynamicGiri`** (`-dgiri`) — an offline analysis pass that reads the trace back and computes the slice.

## Pipeline, mapped onto the normal C toolchain

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
clang -g -O0 -c -emit-llvm file.c -o file.bc
llvm-link *.bc -o program.all.bc
```

`-g` is required: debug info is how the final slice gets mapped back to source `file:line`.

### 2. Instrumentation pass

```sh
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum -trace-giri -trace-file=program.trace \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o program.trace.bc
```

Three passes prepare the ground before instrumentation:
- **`-mergereturn`** normalizes every function to a single return block.
- **`-bbnum`** and **`-lsnum`** assign every basic block and every load/store instruction a
  **stable numeric ID**.

  **This numbering is the glue holding the whole two-phase design together.** It is deterministic on
  a given IR module: instrumentation and slicing must derive **identical IDs** for the same
  instructions. If a port changes iteration order over basic blocks or instructions, numbering can
  silently diverge and slices will silently become wrong without any crash.

Then **`-trace-giri`** injects calls to a runtime library:

| Injected around... | Runtime call | Purpose |
|---|---|---|
| top of every basic block | `RecordBB` / `RecordStartBB` | logs BB entry (its numeric ID) |
| `load` | `RecordLoad` | logs the concrete memory address + size read |
| `store` | `RecordStore` | logs the concrete memory address + size written |
| `select` | `RecordSelect` | logs which operand was chosen |
| `call` / special externals | `RecordCall`, `RecordExtCall`, etc. | logs call sites and memory effects |
| `ret` | `RecordReturn` | logs function return |
| pthread-created functions | `RecordHandlerThreadID`, etc. | thread and lock bookkeeping |

### 3. Ordinary codegen + link, but against the tracing runtime

```sh
llc -O0 program.trace.bc -o program.trace.s
clang++ program.trace.s -o program.trace.exe -lrtgiri
```

Each `Record*` call appends a fixed-size `Entry` record (`include/Giri/Runtime.h`) to a
binary trace file.

**The `Entry` record format** (`include/Giri/Runtime.h`):

```c
enum class RecordType : unsigned {
  BBType = 'B', LDType = 'L', STType = 'S',
  CLType = 'C', RTType = 'R', ENType = 'E', PDType = 'P'
};

struct Entry {
  RecordType type;
  unsigned   id;
  pthread_t  tid;
  uintptr_t  address;
  uintptr_t  length;
  // padding to keep sizeof(Entry) a divisor of page size
};
```

**Porting note:** if a port changes this struct's layout or size, it invalidates the size-divisibility
invariant. Treat this struct as an ABI boundary.

### 4. Run the instrumented binary on a real input

```sh
./program.trace.exe < real_input
```

Produces the `.trace` file: a chronological, binary log of every basic block entered, every
load/store address touched, and every call/return.

### 5. Slicing: a second, offline analysis pass over the same IR + the trace

```sh
opt -load libdgutility.so -load libgiri.so \
    -mergereturn -bbnum -lsnum \
    -dgiri -trace-file=program.trace -slice-file=program.slice \
    -criterion-loc=file.c:42 \
    -remove-bbnum -remove-lsnum \
    program.all.bc -o /dev/null
```

Note this reruns `-mergereturn`/`-bbnum`/`-lsnum` on the **original, non-instrumented** IR
(`program.all.bc`) — determinism of the numbering passes is what makes the resulting IDs line up
with what's sitting in the trace file.

`DynamicGiri` (`include/Giri/Giri.h`, `lib/Giri/Giri.cpp`) is a `ModulePass` that:
1. **Loads the trace** via `TraceFile` (`lib/Giri/TraceFile.cpp`) — `mmap`s the trace and implements
   search operations (`findPreviousID`, `findAllStoresForLoad`).
2. **Resolves the slicing criterion** to a specific dynamic occurrence in the trace (`DynValue`).
3. **Runs a worklist-based backward traversal** (`findSlice`) following:
   - **Data dependence:** static SSA def-use chains + dynamic memory aliasing (via trace).
   - **Control dependence:** `PostDominatorTree` + `PostDominanceFrontier` → `findExecForcers`.
4. Accumulates static `std::set<Value*> Slice` and dynamic
   `std::unordered_set<DynValue> DynSlice` / `std::set<DynValue*> DataFlowGraph`.

### 6. Mapping back to source lines

`SourceLineMappingPass` uses debug info (`locateSrcInfo`) to translate each sliced instruction to
`file:line`. `DynamicGiri` writes these to the `.slice` file, and the test Makefile reduces
them to a sorted, de-duplicated list of source line numbers — the final answer that `make test`
diffs against the checked-in `ans-*.txt` answer keys.

## Key invariants a port must preserve

1. **Numbering determinism across passes.** `-bbnum`/`-lsnum` must assign the same IDs whether run
   during instrumentation or slicing, given equivalent IR.
2. **`Entry` struct ABI stability** — same struct layout, same size-divides-page-size invariant.
3. **Debug info availability and shape.** `SourceLineMapping` depends on `-g` debug info.
4. **Pass registration and ordering.** Pipeline hard-codes pass ordering via explicit
   `opt -load ... -passname` flags; `getAnalysisUsage` must stay consistent.

## Where to look for what

| Question | File |
|---|---|
| How is a BB/load/store numbered? | `include/Utility/BasicBlockNumbering.h`, `LoadStoreNumbering.h` |
| What gets instrumented, and how? | `lib/Giri/TracingNoGiri.cpp` |
| What does the runtime actually write? | `runtime/Giri/Tracing.cpp`, `include/Giri/Runtime.h` |
| How is the trace file parsed? | `lib/Giri/TraceFile.cpp` (~1500 lines, most subtle) |
| How is the backward slice computed? | `lib/Giri/Giri.cpp` |
| How does control dependence get computed? | `include/Utility/PostDominanceFrontier.h` + `PostDominatorTree` |
| How does a slice get back to `file:line`? | `lib/Utility/SourceLineMapping.cpp` |
| Exact build/trace/slice commands | `test/Makefile.common` |
| Dump a trace file for debugging | `tools/PrintTrace/` (`prtrace`) |