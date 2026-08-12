# test2

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** a b c d   **Criterion:** (none — CRITERION empty)
- **Diff:** empty
- **Root cause:** none — test passes cleanly on LLVM 5.0.2

## What the test does
`ifelse.c` defines `func(a)` returning `a + 3`, and `main()` that compares `argc` against `func(argc + 3)`. With `argc = 5` (four arguments "a b c d" plus the program name), `func(8)` yields 11, so `5 < 11` is true and `x = argc - 3 = 2` is the volatile store. The program exits with `x` (2) as the return code. With no criterion location specified, the default criterion is the `ret` instruction of `main`. The golden slice is source lines 5 (the addition inside `func`), 10 (the condition with `func` call), 11 (the then-branch subtraction), and 14 (the return).

## Stage-by-stage output
All stages completed with no warnings or errors.

### Stage 1: `make clean -s -C /giri/test/UnitTests/test2`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm ifelse.c -o ifelse.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link ifelse.bc -o ifelse.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... ifelse.all.bc -o ifelse.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 ifelse.trace.bc -o ifelse.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing ifelse.trace.s -o ifelse.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./ifelse.trace.exe a b c d`
Stdout: empty.
Stderr: empty.
Exit code: 2 (correct — `argc=5`, condition true, `x = 5 - 3 = 2`).

### Stage 8: `opt ... -dgiri ... ifelse.all.bc -o /dev/null`
No output (stdout or stderr).

### Stage 9: `sed ... ifelse.slice | awk ... | sort -g | uniq > ifelse.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff ifelse.slice.loc ans-inst.txt`
Empty diff — files are identical. Both contain lines `5`, `10`, `11`, `14`.

## Diff against golden
Empty — no differences.

## Root causes
None. The test passes cleanly on the LLVM 5.0.2 port.

## Proposed fix
None required.