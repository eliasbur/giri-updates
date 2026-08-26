//===- Giri.h - Dynamic Slicing Pass ----------------------------*- C++ -*-===//
//
//                     Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files defines passes that are used for dynamic slicing.
//
// New pass manager port (LLVM 14.0.0): every Giri pass is a new-PM pass.
//  - TracingNoGiri: a *module pass* (it inserts module-level prototypes/ctor,
//    so it must see the whole module once; it drives the per-function and
//    per-basic-block instrumentation loops itself, preserving the legacy
//    FunctionPass ordering).
//  - DynamicGiri: a *module analysis* whose run() performs the backwards-slice
//    computation (exactly the legacy runOnModule body); it is exposed to the
//    -passes pipeline as the "dgiri" module pass (a thin wrapper). It becomes
//    an analysis so that other passes (e.g. TestGiri) can obtain one shared
//    instance via MAM.getResult<DynamicGiri>(M), mirroring the legacy
//    getAnalysis<DynamicGiri>().
//  - TestGiri: a *module pass* (the legacy -test-giri; not part of the
//    automated suite, kept for parity).
//
//===----------------------------------------------------------------------===//

#ifndef GIRI_H
#define GIRI_H

#include "Giri/TraceFile.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"
#include "Utility/PostDominanceFrontier.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/DataLayout.h"

#include <deque>
#include <set>
#include <unordered_set>

using namespace dg;
using namespace llvm;

namespace giri {

/// This class defines a pass that instruments a program to generate a trace
/// of its execution usable for dynamic slicing.
///
/// The legacy PM exposed this as a FunctionPass with a module-level
/// doInitialization. Under the new PM it is a *module pass*: run() performs
/// the module-level initialization once and then iterates the module's
/// functions and basic blocks itself, preserving the legacy instrumentation
/// order.
class TracingNoGiri : public PassInfoMixin<TracingNoGiri>,
                      public InstVisitor<TracingNoGiri> {
public:
  /// Entry point for this new-PM module pass.
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

  /// This method starts execution of the dynamic slice tracing instrumentation
  /// for one function: it adds code to the function that records the
  /// execution of its basic blocks. Drives the per-BB loop (the legacy
  /// FunctionPass::runOnFunction body, renamed for the new PM).
  void instrument(Function &F);

  /// Instrument a single basic block. Drove the pass in LLVM <= 8 when
  /// TracingNoGiri was a BasicBlockPass; the new-PM run() loop drives it.
  bool runOnBasicBlock(BasicBlock &BB);

  /// Visit a load instruction. This method instruments the load instruction
  /// with a call to the tracing run-time that will record, in the dynamic
  /// trace, the memory read by this load instruction.
  void visitLoadInst(LoadInst &LI);

  /// Visit a store instruction. This method instruments the store instruction
  /// with a call to the tracing run-time that will record, in the dynamic
  /// trace, the memory written by this store instruction.
  void visitStoreInst(StoreInst &SI);

  /// Visit a call instruction. For most call instructions, we will instrument
  /// the call so that the trace contains enough information to track back from
  /// the callee to the caller. For some call instructions, we will emit
  /// special instrumentation to record the memory read and/or written by the
  /// call instruction. Also call records are needed to map invariant failures
  /// to call insts.
  void visitCallInst(CallInst &CI);

  /// Visit a select instruction.  This method instruments the select
  /// instruction with a call to the tracing run-time that will record, in the
  /// dynamic trace, the boolean value that the select instruction will use to
  /// select its output operand.
  void visitSelectInst(SelectInst &SI);

  /// Examine a call instruction and see if it is a call to an external function
  /// which is treated specially by the dynamic slicing code. If so, instrument
  /// it with the appropriate calls to the run-time.
  ///
  /// \param CI - The call instruction which may call a special function.
  /// \return true if this call does call a special call instruction,
  /// otherwise false.
  bool visitSpecialCall(CallInst &CI);

private:
  // Pointers to other passes / analyses
  const DataLayout *TD;
  const QueryBasicBlockNumbers *bbNumPass;
  const QueryLoadStoreNumbers  *lsNumPass;

  // Functions for recording events during execution
  Function *RecordBB;
  Function *RecordStartBB;
  Function *RecordLoad;
  Function *RecordStore;
  Function *RecordSelect;
  Function *RecordStrLoad;
  Function *RecordStrStore;
  Function *RecordStrcatStore;
  Function *RecordCall;
  Function *RecordReturn;
  Function *RecordExtCall;
  Function *RecordExtFun;
  Function *RecordExtCallRet;
  Function *RecordHandlerThreadID;
  Function *Init;
  Function *RecordLock;
  Function *RecordUnlock;

  // Integer types
  Type *Int8Type;
  Type *Int32Type;
  Type *Int64Type;
  Type *VoidType;
  Type *VoidPtrType;

private:
  /// Module-level initialization: insert the record-function prototypes and
  /// the global constructor. The new-PM equivalent of the legacy
  /// doInitialization (called once per module from run()).
  void initializeDataStructure(Module &M);

  /// Instrument the unlock function for load/store instructions
  /// This should insert a function call after the I;
  void instrumentLock(Instruction *I);

  /// Instrument the unlock function for load/store instructions
  /// This should insert a function call after the I;
  void instrumentUnlock(Instruction *I);

  /// This method instruments a basic block so that it records its execution at
  /// run-time.
  void instrumentBasicBlock(BasicBlock &BB);

  /// Create a global constructor (ctor) function that can be called when the
  /// program starts up.
  void createCtor(Module &M);
};

/// This pass finds the backwards dynamic slice of LLVM values.
///
/// New-PM: implemented as a *module analysis* so that it can be obtained as a
/// shared instance via MAM.getResult<DynamicGiri>(M) (mirroring the legacy
/// getAnalysis<DynamicGiri>()), and so it can be exposed to the -passes
/// pipeline as the "dgiri" module pass.
class DynamicGiri : public AnalysisInfoMixin<DynamicGiri> {
  friend AnalysisInfoMixin<DynamicGiri>;

  static AnalysisKey Key;

public:
  using Result = DynamicGiri;

  /// Using trace information, find the dynamic backwards slice of a specified
  /// LLVM instruction. (the legacy runOnModule body).
  DynamicGiri run(Module &M, ModuleAnalysisManager &MAM);

  /// This method returns all of the values that are in the backwards slice of
  /// the specified instruction.
  void getBackwardsSlice(Instruction *I,
                         std::set<Value *> &Slice,
                         std::unordered_set<DynValue> &DynSlice,
                         std::set<DynValue *> &DataFlowGraph);

  /// This method prints all of the values that are in the backwards slice of
  /// the specified instruction.
  void printBackwardsSlice(const Instruction *Criterion,
                           std::set<Value *> &Slice,
                           std::unordered_set<DynValue> &DynSlice,
                           std::set<DynValue *> &DataFlowGraph);

private:
  typedef std::deque<DynValue *> Worklist_t;
  typedef std::set<DynValue *> Processed_t;

  /// For the given value, find all of the values upon which it depends.
  void findSlice(DynValue &Initial,
                 std::unordered_set<DynValue> &DynSlice,
                 std::set<DynValue *> &DataFlowGraph);

  /// Find the basic blocks that can force execution of the specified basic
  /// block and return the identifiers used to represent those basic blocks
  /// within the dynamic trace.
  bool findExecForcers(BasicBlock *BB, std::set<unsigned> &bbNums);

  void ensurePostDomFrontierComputed(Function &F);
  llvm::DenseMap<Function*, PostDominanceFrontier*> FunctionPDFFrontiers;
  llvm::DenseMap<Function*, PostDominatorTree*> FunctionPDTs;

  void initDataFlowFitler(void);

  bool checkType(const Type *T);

  /// Trace file object (used for querying the trace)
  TraceFile *Trace;

  /// Cache of basic blocks that force execution of other basic blocks
  std::map<BasicBlock *, std::vector<BasicBlock *> > ForceExecCache;
  std::map<BasicBlock *, bool> ForceAtLeastOnceCache;

  /// Passes used by this pass
  const QueryBasicBlockNumbers *bbNumPass;
  const QueryLoadStoreNumbers *lsNumPass;
};

/// Thin module pass that runs the DynamicGiri analysis (the "dgiri" pipeline
/// name). The analysis performs all the work; this wrapper only triggers it.
class DynamicGiriPass : public PassInfoMixin<DynamicGiriPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    MAM.getResult<DynamicGiri>(M);
    return PreservedAnalyses::all();
  }
};

/// This pass is used to test the giri pass (the legacy -test-giri).
class TestGiri : public PassInfoMixin<TestGiri> {
public:
  /// Entry point for this pass. Find the instruction specified by the user
  /// and find the backwards slice of it.
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

  StringRef getPassName() const {
    return "Dynamic Backwards Slice Testing Pass";
  }

private:
  // Dynamic backwards slice
  std::set<Value *> mySliceOfLife;
  std::unordered_set<DynValue> myDynSliceOfLife;
  std::set<DynValue *> myDataFlowGraph;
};

} // END namespace giri

#endif
