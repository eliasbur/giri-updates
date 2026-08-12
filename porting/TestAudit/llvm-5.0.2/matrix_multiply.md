# matrix_multiply

- **Verdict:** FAIL-BUG
- **Golden file:** ans-inst-pthread.txt   **Input:** 4   **Criterion:** -criterion-inst=criterion-inst-pthread.txt (inst: matrixmult_map 138)
- **Diff:** 25 lines missing / 0 lines extra
- **Root cause:** PostDominatorFrontier.cpp:37 early-return at virtual root causes "Could not find Control-dep" warnings (30 occurrences), producing an incomplete backward slice that drops 25 of 60 golden source lines.

## What the test does
- **Source:** `matrix_multiply-pthread.c` — pthread-based MapReduce matrix multiplication. `main()` sets up shared memory, calls `matrixmult_splitter()` which spawns 4 threads that each run `matrixmult_map()`.
- **Computation:** Each thread computes a set of rows of the output matrix C = A × B. The inner loop at `matrixmult_map:138` assigns `data->output[x_loc*data->matrix_len + i] = value`.
- **Criterion:** Instruction-level criterion targeting `matrixmult_map:138` — the store of a computed cell into the output matrix.

## Stage-by-stage output
| Stage | Command | Result |
|-------|---------|--------|
| 1 | `make clean -s` | Clean, no output |
| 2 | `clang -g -O0 -c -emit-llvm` | Clean compilation |
| 3 | `llvm-link` | Clean link |
| 4 | `opt … -trace-giri …` | Clean instrumentation pass |
| 5 | `llc -asm-verbose=false -O0` | Clean assembly generation |
| 6 | `clang++ … -lrtgiri -lpthread` | Clean link |
| 7 | `./matrix_multiply-pthread.trace.exe 4` | Exit 0. stderr: `***** file size is 64` |
| 8 | `opt … -dgiri …` | **30x "Could not find Control-dep of this Basic Block"** (stderr). Criterion resolved: `Start slicing Function:Instruction is defined as matrixmult_map:138`. Also: `For some variable length functions like ap_rprintf in apache, call records missing.` + `i8* %0` |
| 9 | `sed … > slice.loc` | 35 lines extracted |
| 10 | `diff slice.loc ans-inst-pthread.txt` | Exit 1. 25 lines in golden not in slice, 0 extra |

## Diff against golden
25 lines present in `ans-inst-pthread.txt` but absent from `matrix_multiply-pthread.slice.loc`:

```
> 51    (out->data = data_out)
> 52    (pthread_attr_setscope)
> 53    (req_rows computation)
> 75    (b_ptr increment)
> 76    (value += a_ptr[j] * (*b_ptr))
> 77    (b_ptr += data->matrix_len)
> 78    (x_loc assignment)
> 79    (y_loc assignment)
> 84    (row_count++)
> 92    (matrix_len = atoi)
> 98    (open fd_out)
> 102   (fd_B open)
> 103   (fstat fd_B)
> 109   (fdata_B mmap)
> 133   (b_ptr = data->matrix_B + i)
> 136   (value += a_ptr[j] * (*b_ptr) — inner multiply)
> 138   (data->output[...]=value — the criterion line itself)
> 179   (dprintf "\n" in output loop)
> 187   (dprintf "%d " in output loop)
> 193   (dprintf "\n" after output)
> 196   (free mm_data.output)
> 201   (munmap fdata_A)
> 204   (munmap fdata_B)
> 242   (munmap fdata_A — second unmap)
> 251   (munmap fdata_B — second unmap)
```

## Root causes
1. **PostDominatorFrontier.cpp:37 — Early return at virtual root.** The function hits an early return when analyzing the virtual (artificial) root node of the post-dominator tree, causing 30 "Could not find Control-dep" failures. These failures break the control-dependence computation for a significant portion of the basic blocks, resulting in an incomplete backward slice. Lines from the pthread management code, the inner multiplication loop, criterion line itself, and cleanup code are all dropped. This is the known bug confirmed present in stage 8 stderr.

2. **Secondary noise:** "For some variable length functions like ap_rprintf in apache, call records missing" — the trace file lacks complete call records for some variable-length function calls. This may compound the slicing incompleteness but is a separate issue.

## Proposed fix
Fix the early return at `PostDominatorFrontier.cpp:37` so the virtual root node participates in post-dominator frontier computation rather than causing control-dependence lookup failures. The fix must preserve the numbering determinism and Entry struct ABI invariants. Additionally, investigate whether the missing call records for variable-length functions contribute to the dropped slices and address if needed.