//===- TracingNoGiri.cpp - Dynamic Slicing Trace Instrumentation Pass -----===//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files defines passes that are used for dynamic slicing.
//
// TODO:
// Technically, we should support the tracing of signal handlers.  This can
// interrupt the execution of a basic block.
//
//===----------------------------------------------------------------------===//


#include "Giri/Giri.h"
#include "Utility/Utils.h"
#include "Utility/VectorExtras.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <vector>
#include <string>

using namespace giri;
using namespace llvm;

extern llvm::cl::opt<std::string> TraceFilename;

// DEBUG_TYPE is defined after the includes: LLVM 14's dom-tree-builder header
// (GenericDomTreeConstruction.h) does `#undef DEBUG_TYPE`, and the 14.0.0 STATISTIC
// macro references DEBUG_TYPE at the call site.
#define DEBUG_TYPE "giri"
STATISTIC(NumBBs, "Number of basic blocks");
STATISTIC(NumPHIBBs, "Number of basic blocks with phi nodes");
STATISTIC(NumLoads, "Number of load instructions processed");
STATISTIC(NumStores, "Number of store instructions processed");
STATISTIC(NumSelects, "Number of select instructions processed");
STATISTIC(NumLoadStrings, "Number of load instructions processed");
STATISTIC(NumStoreStrings, "Number of store instructions processed");
STATISTIC(NumCalls, "Number of call instructions processed");
STATISTIC(NumExtFuns, "Number of special external calls processed, e.g. memcpy");

// New-PM: TracingNoGiri is registered as the "trace-giri" pipeline pass in
// GiriPassPlugin.cpp (the legacy RegisterPass is removed).

static inline Function *getOrInsertF(Module &M,
                                       StringRef Name,
                                       Type *RetTy,
                                       ArrayRef<Type *> Args = {})
  FunctionType *FTy = FunctionType::get(RetTy, Args, false);
  return cast<Function>(M.getOrInsertFunction(Name, FTy).getCallee());
}

static bool hasPHI(const BasicBlock & BB) {
  for (BasicBlock::const_iterator I = BB.begin(); I != BB.end(); ++I)
    if (isa<PHINode>(I)) return true;
  return false;
}

// New-PM module pass entry point: perform the module-level initialization once
// (the legacy doInitialization), then drive the per-function and per-basic-block
// instrumentation loops (the legacy FunctionPass::runOnFunction /
// BasicBlockPass::runOnBasicBlock order, preserved so the instrumentation order
// is unchanged).
PreservedAnalyses TracingNoGiri::run(Module &M, ModuleAnalysisManager &MAM) {
  TD        = &M.getDataLayout();
  bbNumPass = &MAM.getResult<QueryBBNumbersPass>(M);
  lsNumPass = &MAM.getResult<QueryLSNumbersPass>(M);

  initializeDataStructure(M);

  for (Module::iterator F = M.begin(); F != M.end(); ++F)
    instrument(*F);

  return PreservedAnalyses::all();
}

void TracingNoGiri::initializeDataStructure(Module & M) {
  Int8Type  = IntegerType::getInt8Ty(M.getContext());
  Int32Type = IntegerType::getInt32Ty(M.getContext());
  Int64Type = IntegerType::getInt64Ty(M.getContext());
  VoidPtrType = PointerType::getUnqual(Int8Type);
  VoidType = Type::getVoidTy(M.getContext());

Init = getOrInsertF(M, "recordInit", VoidType, {VoidPtrType});
  RecordLock = getOrInsertF(M, "recordLock", VoidType, {VoidPtrType});
  RecordUnlock = getOrInsertF(M, "recordUnlock", VoidType, {VoidPtrType});
  RecordBB = getOrInsertF(M, "recordBB", VoidType, {Int32Type, VoidPtrType, Int32Type});
  RecordStartBB = getOrInsertF(M, "recordStartBB", VoidType, {Int32Type, VoidPtrType});
  RecordLoad = getOrInsertF(M, "recordLoad", VoidType, {Int32Type, VoidPtrType, Int64Type});
  RecordStrLoad = getOrInsertF(M, "recordStrLoad", VoidType, {Int32Type, VoidPtrType});
  RecordStore = getOrInsertF(M, "recordStore", VoidType, {Int32Type, VoidPtrType, Int64Type});
  RecordStrStore = getOrInsertF(M, "recordStrStore", VoidType, {Int32Type, VoidPtrType});
  RecordStrcatStore = getOrInsertF(M, "recordStrcatStore", VoidType, {Int32Type, VoidPtrType, VoidPtrType});
  RecordCall = getOrInsertF(M, "recordCall", VoidType, {Int32Type, VoidPtrType});
  RecordExtCall = getOrInsertF(M, "recordExtCall", VoidType, {Int32Type, VoidPtrType});
  RecordReturn = getOrInsertF(M, "recordReturn", VoidType, {Int32Type, VoidPtrType});
  RecordExtCallRet = getOrInsertF(M, "recordExtCallRet", VoidType, {Int32Type, VoidPtrType});
  RecordSelect = getOrInsertF(M, "recordSelect", VoidType, {Int32Type, Int8Type});
  createCtor(M);
}

void TracingNoGiri::createCtor(Module &M) {
  // Create the ctor function.
  Type *VoidTy = Type::getVoidTy(M.getContext());
  Function *RuntimeCtor = getOrInsertF(M, "giriCtor", VoidTy);
  assert(RuntimeCtor && "Somehow created a non-function function!\n");

  RuntimeCtor->addFnAttr(Attribute::NoUnwind);
  RuntimeCtor->setLinkage(GlobalValue::InternalLinkage);

  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry", RuntimeCtor);
  Constant *Name = stringToGV(TraceFilename, &M);
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  std::vector<Value*> ctorArgs;
  ctorArgs.push_back(Name);
  CallInst::Create(Init, ArrayRef<Value*>(ctorArgs.data(), ctorArgs.size()), "", BB);

  ReturnInst::Create(M.getContext(), BB);

  appendToGlobalCtors(M, RuntimeCtor, 65535);
}

void TracingNoGiri::instrumentLock(Instruction *I) {
  std::string s;
  raw_string_ostream rso(s);
  I->print(rso);
  Constant *Name = stringToGV(rso.str(),
                              I->getParent()->getParent()->getParent());
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  {
    std::vector<Value*> lockArgs;
    lockArgs.push_back(Name);
    CallInst::Create(RecordLock, ArrayRef<Value*>(lockArgs.data(), lockArgs.size()))->insertBefore(I);
  }
}

void TracingNoGiri::instrumentUnlock(Instruction *I) {
  std::string s;
  raw_string_ostream rso(s);
  I->print(rso);
  Constant *Name = stringToGV(rso.str(),
                              I->getParent()->getParent()->getParent());
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  {
    std::vector<Value*> unlockArgs;
    unlockArgs.push_back(Name);
    CallInst::Create(RecordUnlock, ArrayRef<Value*>(unlockArgs.data(), unlockArgs.size()))->insertAfter(I);
  }
}

void TracingNoGiri::instrumentBasicBlock(BasicBlock &BB) {
  if (BB.getParent()->getName() == "giriCtor")
    return;

  unsigned id = bbNumPass->getID(&BB);
  assert(id && "Basic block does not have an ID!\n");
  Value *BBID = ConstantInt::get(Int32Type, id);

  Value *FP = castTo(BB.getParent(), VoidPtrType, "", BB.getTerminator());

  Value *LastBB;
  if (isa<ReturnInst>(BB.getTerminator()))
     LastBB = ConstantInt::get(Int32Type, 1);
  else
     LastBB = ConstantInt::get(Int32Type, 0);

  std::vector<Value *> args = make_vector<Value *>(BBID, FP, LastBB, 0);
  instrumentLock(BB.getTerminator());
  Instruction *RBB = CallInst::Create(RecordBB, ArrayRef<Value*>(args.data(), args.size()), "", BB.getTerminator());
  instrumentUnlock(RBB);

  args = make_vector<Value *>(BBID, FP, 0);
  Instruction *F = &*BB.getFirstInsertionPt();
  Instruction *S = CallInst::Create(RecordStartBB, ArrayRef<Value*>(args.data(), args.size()), "", F);
  instrumentLock(S);
  instrumentUnlock(S);
}

void TracingNoGiri::visitLoadInst(LoadInst &LI) {
  instrumentLock(&LI);

  Value *LoadID = ConstantInt::get(Int32Type, lsNumPass->getID(&LI));
  Value *Pointer = LI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &LI);
  uint64_t size = TD->getTypeStoreSize(LI.getType());
  Value *LoadSize = ConstantInt::get(Int64Type, size);
  std::vector<Value *> args=make_vector<Value *>(LoadID, Pointer, LoadSize, 0);
  CallInst::Create(RecordLoad, ArrayRef<Value*>(args.data(), args.size()), "", &LI);

  instrumentUnlock(&LI);
  ++NumLoads;
}

void TracingNoGiri::visitSelectInst(SelectInst &SI) {
  instrumentLock(&SI);

  Value *Predicate = SI.getCondition();
  Predicate = castTo(Predicate, Int8Type, Predicate->getName(), &SI);
  Value *SelectID = ConstantInt::get(Int32Type, lsNumPass->getID(&SI));
  std::vector<Value *> args=make_vector<Value *>(SelectID, Predicate, 0);
  CallInst::Create(RecordSelect, ArrayRef<Value*>(args.data(), args.size()), "", &SI);

  instrumentUnlock(&SI);
  ++NumSelects;
}

void TracingNoGiri::visitStoreInst(StoreInst &SI) {
  instrumentLock(&SI);

  Value * Pointer = SI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &SI);
  uint64_t size = TD->getTypeStoreSize(SI.getOperand(0)->getType());
  Value *StoreSize = ConstantInt::get(Int64Type, size);
  Value *StoreID = ConstantInt::get(Int32Type, lsNumPass->getID(&SI));
  std::vector<Value *> args=make_vector<Value *>(StoreID, Pointer, StoreSize, 0);
  CallInst::Create(RecordStore, ArrayRef<Value*>(args.data(), args.size()), "", &SI);

  instrumentUnlock(&SI);
  ++NumStores;
}

bool TracingNoGiri::visitSpecialCall(CallInst &CI) {
  Function *CalledFunc = CI.getCalledFunction();

  if (CalledFunc == nullptr)
    return false;

  if (!CalledFunc->isDeclaration())
    return false;

  std::string name = CalledFunc->getName().str();
  if (name.substr(0,12) == "llvm.memset.") {
    instrumentLock(&CI);

    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    Value *NumElts = CI.getOperand(2);
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst::Create(RecordStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

    instrumentUnlock(&CI);
    ++NumExtFuns;
    return true;
  } else if (name.substr(0,12) == "llvm.memcpy." ||
             name.substr(0,13) == "llvm.memmove." ||
             name == "strcpy") {
    instrumentLock(&CI);

    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    if(name == "strcpy") {
      std::vector<Value *> args = make_vector(CallID, srcPointer, 0);
      CallInst::Create(RecordStrLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

      args = make_vector(CallID, dstPointer, 0);
      CallInst *recStore = CallInst::Create(RecordStrStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
      CI.moveBefore(recStore);
    } else {
      Value *NumElts = CI.getOperand(2);
      std::vector<Value *> args = make_vector(CallID, srcPointer, NumElts, 0);
      CallInst::Create(RecordLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

      args = make_vector(CallID, dstPointer, NumElts, 0);
CallInst::Create(RecordStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
    }

    instrumentUnlock(&CI);
    ++NumExtFuns;
    return true;
  } else if (name == "strcat") {
    instrumentLock(&CI);

    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst::Create(RecordStrLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

    args = make_vector(CallID, srcPointer, 0);
    CallInst::Create(RecordStrLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

    args = make_vector(CallID, dstPointer, srcPointer, 0);
    CallInst::Create(RecordStrcatStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

    instrumentUnlock(&CI);
    ++NumExtFuns;
    return true;
  } else if (name == "strlen") {
    instrumentLock(&CI);

    Value *srcPointer  = CI.getOperand(0);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    std::vector<Value *> args = make_vector(CallID, srcPointer, 0);
    CallInst::Create(RecordStrLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

    instrumentUnlock(&CI);
    ++NumExtFuns;
    return true;
  } else if (name == "calloc") {
    instrumentLock(&CI);

    Value *NumElts = BinaryOperator::Create(BinaryOperator::Mul,
                                            CI.getOperand(0),
                                            CI.getOperand(1),
                                            "calloc par1 * par2",
                                            &CI);
    Value *dstPointer = castTo(&CI, VoidPtrType, CI.getName(), &CI);

    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
    CI.moveBefore(recStore);

    CI.moveBefore(cast<Instruction>(NumElts));

    instrumentUnlock(&CI);
    ++NumExtFuns;
    return true;
  } else if (name == "tolower" || name == "toupper") {
  } else if (name == "fscanf") {
  } else if (name == "sscanf") {
  } else if (name == "sprintf") {
    instrumentLock(&CI);
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    for (unsigned index = 2; index < CI.getNumOperands(); ++index) {
      if (CI.getOperand(index)->getType() == VoidPtrType) {
        Value *Ptr = CI.getOperand(index);
        std::vector<Value *> args = make_vector(CallID, Ptr, 0);
CallInst::Create(RecordStrLoad, ArrayRef<Value*>(args.data(), args.size()), "", &CI);

        ++NumLoadStrings;
      }
    }

    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
    CI.moveBefore(recStore);

    instrumentUnlock(&CI);
    ++NumStoreStrings;
    return true;
  } else if (name == "fgets") {
    instrumentLock(&CI);

    Value * dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    Value * CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrStore, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
    CI.moveBefore(recStore);

    instrumentUnlock(&CI);
    ++NumStoreStrings;
    return true;
  }

  return false;
}

void TracingNoGiri::visitCallInst(CallInst &CI) {
  Function *CalledFunc = CI.getCalledFunction();
  if (!CalledFunc)
    return;

  if (isTracerFunction(CalledFunc))
    return;

  if (!CalledFunc->getName().str().compare(0,9,"llvm.dbg."))
    return;

  if (CalledFunc->isDeclaration() && CalledFunc->isIntrinsic()) {
     visitSpecialCall(CI);
     return;
  }

  if (isa<InlineAsm>(CI.getCalledOperand()->stripPointerCasts()))
    return;

  instrumentLock(&CI);
  Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
  Value *FP = castTo(CI.getCalledOperand(), VoidPtrType, "", &CI);
  std::vector<Value *> args = make_vector<Value *>(CallID, FP, 0);
  Instruction *RC;
  if (CalledFunc->isDeclaration())
    RC = CallInst::Create(RecordExtCall, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
  else
    RC = CallInst::Create(RecordCall, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
  instrumentUnlock(RC);

  CallInst *RI = CallInst::Create(RecordReturn, ArrayRef<Value*>(args.data(), args.size()), "", &CI);
  CI.moveBefore(RI);
  instrumentLock(RI);
  instrumentUnlock(RI);

  ++NumCalls;

  visitSpecialCall(CI);
}

// LLVM 9 removed the legacy BasicBlockPass, which previously called
// runOnBasicBlock once per basic block. The new-PM run() drives this loop.
void TracingNoGiri::instrument(Function &F) {
  for (Function::iterator I = F.begin(); I != F.end(); ++I)
    runOnBasicBlock(*I);
}

bool TracingNoGiri::runOnBasicBlock(BasicBlock &BB) {
  // TD/bbNumPass/lsNumPass are set once in run() (the new-PM equivalent of the
  // legacy per-BB getAnalysis<...>(); the numbering analyses are module-level).
  instrumentBasicBlock(BB);

  std::vector<Instruction *> Worklist;
  for (BasicBlock::iterator I = BB.begin(); I != BB.end(); ++I)
    Worklist.push_back(&*I);
  visit(Worklist.begin(), Worklist.end());

  if (hasPHI(BB))
    ++NumPHIBBs;

  ++NumBBs;

  return true;
}