# matrix_multiply-seq

- **Verdict:** FAIL-EXPECTED
- **Golden file:** ans-inst-seq.txt   **Input:** 4   **Criterion:** matrix_mult 285
- **Diff:** 0 lines missing from golden / 16 lines extra in actual; 1 line from golden absent in actual
- **Root cause:** LLVM 5.0.2 generates different IR than LLVM 3.4, producing different basic block layout and instruction numbering; golden file is tied to a specific compiler build

## What the test does
`matrix_multiply-seq.c` loads two integer arrays from files, multiplies them using a block-serial matrix multiplication (`matrix_mult`), and prints the result. The slicing criterion is `matrix_mult:285` (the 285th LLVM instruction in the `matrix_mult` function). The golden slice covers lines 56, 90, 97, 113, 119-123, 129, 131, 133, 137, 139, 141, 145, 147, 149, 154 (19 lines).

## Stage-by-stage output

### Stage 1: `make clean`
No output (stdout or stderr).

### Stage 2: `clang -g -O0 -c -emit-llvm matrix_multiply-seq.c -o matrix_multiply-seq.bc`
No output (stdout or stderr).

### Stage 3: `llvm-link matrix_multiply-seq.bc -o matrix_multiply-seq.all.bc`
No output (stdout or stderr).

### Stage 4: `opt ... -trace-giri ... matrix_multiply-seq.all.bc -o matrix_multiply-seq.trace.bc`
No output (stdout or stderr).

### Stage 5: `llc -asm-verbose=false -O0 matrix_multiply-seq.trace.bc -o matrix_multiply-seq.trace.s`
No output (stdout or stderr).

### Stage 6: `clang++ -fno-strict-aliasing matrix_multiply-seq.trace.s -o matrix_multiply-seq.trace.exe -L/giri/build/lib -lrtgiri`
No output (stdout or stderr).

### Stage 7: `./matrix_multiply-seq.trace.exe 4`
Stdout: 4×4 matrix printout + multiplication result (4×4 product matrix). Exit code 0.
Stderr: empty.

### Stage 8: `opt ... -dgiri ... matrix_multiply-seq.all.bc -o /dev/null`
Stderr: `Start slicing Function:Instruction is defined as matrix_mult:285`
No warnings. After DenseMap→std::map fix, 0 "Could not find Control-dep" messages (previously 13).

### Stage 9: `sed ... matrix_multiply-seq.slice | awk ... | sort -g | uniq > matrix_multiply-seq.slice.loc`
No output (stdout or stderr).

### Stage 10: `diff matrix_multiply-seq.slice.loc ans-inst-seq.txt`
Non-empty diff. See "Diff against golden" below.

## Diff against golden

Actual output contains 35 unique source lines. Golden file contains 19 lines.

Lines in actual output but not in golden (16 extra): 75, 76, 77, 79, 80, 81, 82, 84, 85, 86, 92, 94, 155, 156, 157, 165. These correspond to:
- Lines 75-86, 92-94: matrix multiplication inner loop body (loop headers, boundary calculations, loads, multiplications, stores)
- Lines 155-157, 165: main function data setup (pointer assignments to matrix_A, matrix_B, matrix_out; memset)

Line in golden but absent from actual (1 missing): 97 (`dprintf("\n")` — newline print in output formatting).

Every line in the golden that appears in the actual output (18 remaining golden lines) matches exactly.

## Root causes

1. **Compiler IR differences between LLVM 3.4 and LLVM 5.0.2.** (FAIL-EXPECTED) The golden file `ans-inst-seq.txt` was generated against an LLVM 3.4 build. LLVM 5.0.2's clang emits a different instruction sequence, basic block layout, and debug-info mapping for `matrix_multiply-seq.c`. The slicing criterion (`matrix_mult:285`) resolves to a different instruction in LLVM 5.0.2's IR, and the backward slice therefore traces through a different set of instructions. The 16 extra lines in the actual output represent instructions that are data/control-dependent on the LLVM 5.0.2 criterion instruction but were not in the slice when the golden was generated with LLVM 3.4. The 1 missing line (97) represents an instruction that WAS included in the LLVM 3.4 criterion's slice but is NOT reached from the LLVM 5.0.2 criterion. This difference cannot be resolved without changing the golden file, the criterion, or the compiler version.

2. **DenseMap reference invalidation during recursive `calculate()`.** (fixed by this task) Before the fix, `Frontiers` was `DenseMap<BasicBlock*, DomSetType>`. The recursive `calculate()` function held a reference `DomSetType &S = Frontiers[BB]` across recursive calls that inserted new keys. DenseMap invalidates all references on rehash. For `matrix_mult`'s post-dominator tree (enough BBs to trigger rehash), this caused 13 "Could not find Control-dep" warnings and corrupted frontier data. The fix changed `Frontiers` to `std::map<BasicBlock*, DomSetType>`, which provides reference stability. After the fix, 0 warnings and correct frontier computation. Evidence: with DenseMap, 13 "Could not find Control-dep of this Basic Block" messages appeared (captured in log); with std::map, 0 messages. Verified by reverting the fix and re-running: 13 warnings reappeared.

## Proposed fix
No further fix possible for root cause 1 (FAIL-EXPECTED — compiler version difference). Root cause 2 is fixed by changing `Frontiers` from `DenseMap` to `std::map` in `include/Utility/PostDominanceFrontier.h:28`.