# test1

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** Mingliang LIU   **Criterion:** (none — CRITERION empty)
- **Diff:** empty
- **Root cause:** none — test passes cleanly on LLVM 5.0.2

## What the test does
`extlibcalls.c` allocates a 128-byte buffer, copies two command-line arguments into it via `strcpy`/`strcat`, prints the result and its length, then returns the length. The slicing criterion is the `return strlen(input)` instruction (line 18). The test validates that external library calls (`malloc`, `memset`, `strcpy`, `strcat`, `strlen`, `printf`) are traced and sliced correctly. The golden slice is lines 9, 12, 13, 18.

## Stage-by-stage output
All stages completed silently with no warnings or errors.

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm extlibcalls.c -o extlibcalls.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link extlibcalls.bc -o extlibcalls.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... extlibcalls.all.bc -o extlibcalls.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 extlibcalls.trace.bc -o extlibcalls.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing extlibcalls.trace.s -o extlibcalls.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./extlibcalls.trace.exe Mingliang LIU`
Stdout: `MingliangLIU\n12`, exit code 12 (expected — program returns strlen of concatenated string).
Stderr: empty.

### Stage 8: `opt ... -dgiri ... extlibcalls.all.bc -o /dev/null`
No output (stdout or stderr).

### Stage 9: `sed ... extlibcalls.slice | awk ... | sort -g | uniq > extlibcalls.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff extlibcalls.slice.loc ans-inst.txt`
Empty diff — files are identical. Both contain lines `9`, `12`, `13`, `18`.

## Diff against golden
Empty — no differences.

## Root causes
None. The test passes cleanly on the LLVM 5.0.2 port.

## Proposed fix
None required.