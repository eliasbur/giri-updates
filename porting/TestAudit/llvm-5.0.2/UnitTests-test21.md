# test21

- **Verdict:** CLEAN
- **Golden file:** ans-inst.txt   **Input:** t Giri   **Criterion:** -criterion-inst=criterion-inst.txt (main:51)
- **Diff:** empty
- **Root cause:** n/a — test passes without issues

## What the test does

**Source:** `hwtype.c` — command-line argument parser with a switch on `*argv[1]` that dispatches on first character ('A'/'p' or 'H'/'t'), sets `hw` and `ap` pointers, and conditionally calls `get_hwtype()`.

**Computation:** Invoked as `./hwtype.trace.exe t Giri` (argc=3). `*argv[1]` is `'t'`, matching the `'H'`/`'t'` case. `argv[3]` is NULL, so `get_hwtype(NULL)` returns NULL. The post-switch guard `hw_set && *argv[1] != 'H'` is true, so `hw` is reassigned to `get_hwtype("DFLT_HW")`. Output: `(null) DFLT_HW`. Exit code: 0.

**Criterion:** Instruction at `main:51` (the load of `hw` at the `printf` call on source line 25).

## Stage-by-stage output

| Stage | Command | Status | Notes |
|---|---|---|---|
| 1 | `make clean -s` | OK | No output |
| 2 | `clang -g -O0 -c -emit-llvm hwtype.c -o hwtype.bc` | OK | No output |
| 3 | `llvm-link hwtype.bc -o hwtype.all.bc` | OK | No output |
| 4 | `opt ... -trace-giri ...` | OK | No warnings on stderr |
| 5 | `llc -asm-verbose=false -O0 ...` | OK | No output |
| 6 | `clang++ ... -lrtgiri` | OK | No output |
| 7 | `./hwtype.trace.exe t Giri` | OK | Exit 0, stdout: `(null) DFLT_HW` |
| 8 | `opt ... -dgiri ... -criterion-inst=criterion-inst.txt ...` | OK | Stderr: `Start slicing Function:Instruction is defined as main:51` — no "Could not find Control-dep" warning |
| 9 | `sed ... awk ... sort -g uniq > hwtype.slice.loc` | OK | Extracted lines: 4, 11, 18, 23, 24, 25 |
| 10 | `diff hwtype.slice.loc ans-inst.txt` | OK | Exit 0, files identical |

## Diff against golden

None — `hwtype.slice.loc` matches `ans-inst.txt` exactly (6 lines: 4, 11, 18, 23, 24, 25).

## Root causes

N/A — test passes cleanly. Criterion instruction `main:51` resolved correctly; slicing produced the expected set of source lines with no warnings. No "Could not find Control-dep" message on stderr.

## Proposed fix

None.