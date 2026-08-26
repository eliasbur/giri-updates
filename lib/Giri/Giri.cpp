//===- Giri.cpp - Find dynamic backwards slice analysis pass -------------- --//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements an analysis pass that allows clients to find the
// instructions contained within the dynamic backwards slice of a specified
// instruction.
//
// New pass manager port (LLVM 14.0.0): DynamicGiri is now a new-PM *module
// analysis* (see Giri.h). run() is the legacy runOnModule body; the post-
// dominance trees / frontiers it used are computed inline (new
// PostDominatorTree(F) + computeFrontiers) instead of via the removed
// legacy PostDominatorTreeWrapperPass. "dgiri" is exposed to the -passes
// pipeline as a thin module pass (see GiriPassPlugin.cpp).
//
//===----------------------------------------------------------------------===//


#include "Giri/Giri.h"
#include "Utility/SourceLineMapping.h"
#include "Utility/Utils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include <iostream>
#include <fstream>
// DEBUG_TYPE is redefined after the includes: LLVM 14's dom-tree-builder header
// (GenericDomTreeConstruction.h, pulled in by Statistic.h / Dominators.h) does
// `#undef DEBUG_TYPE`, and the 14.0.0 STATISTIC macro references DEBUG_TYPE at the
// call site. 14.0.0's Debug.h also dropped the bare DEBUG(X) macro, so restore it.
#define DEBUG_TYPE "giri"
#define DEBUG(X) DEBUG_WITH_TYPE(DEBUG_TYPE, X)

using namespace llvm;
using namespace giri;

extern cl::opt<std::string> TraceFilename;

static cl::opt<std::string>
SliceFilename("slice-file", cl::desc("Slice output file name"), cl::init("-"));

static cl::opt<std::string>
StartOfSliceLoc("criterion-loc",
                 cl::desc("Define slicing criterion by line of source code"),
                 cl::init(""));

static cl::opt<std::string>
StartOfSliceInst("criterion-inst",
                  cl::desc("Define slicing criterion by instruction number"),
                  cl::init(""));

static cl::opt<bool>
TraceCD("trace-cd", cl::desc("Trace control dependence"), cl::init(true));

STATISTIC(NumDynValues, "Number of Dynamic Values in Slice");
STATISTIC(NumDynSources, "Number of Dynamic Sources Queried");
STATISTIC(NumDynValsSkipped, "Number of Dynamic Values Skipped");
STATISTIC(NumLoadsTraced, "Number of Dynamic Loads Traced");
STATISTIC(NumLoadsLost, "Number of Dynamic Loads Lost");

// New-PM module analysis key (the legacy `char DynamicGiri::ID` is removed).
AnalysisKey DynamicGiri::Key;

void DynamicGiri::ensurePostDomFrontierComputed(Function &F) {
  if (FunctionPDTs.find(&F) != FunctionPDTs.end())
    return;

  // New-PM: build the post-dominator tree directly (the public Function& ctor
  // computes it), replacing the legacy lazy PostDominatorTreeWrapperPass.
  PostDominatorTree *PDT = new PostDominatorTree(F);
  FunctionPDTs[&F] = PDT;

  PostDominanceFrontier *PDF = new PostDominanceFrontier();
  PDF->computeFrontiers(*PDT);
  FunctionPDFFrontiers[&F] = PDF;
}

bool DynamicGiri::findExecForcers(BasicBlock *BB,
                                    std::set<unsigned> &bbNums) {
  Function *F = BB->getParent();
  ensurePostDomFrontierComputed(*F);

  PostDominatorTree &PDT = *FunctionPDTs[F];
  PostDominanceFrontier &PDF = *FunctionPDFFrontiers[F];

  if (ForceExecCache.find(BB) != ForceExecCache.end()) {
    for (unsigned index = 0; index < ForceExecCache[BB].size(); ++index) {
      BasicBlock *ForcerBB = ForceExecCache[BB][index];
      bbNums.insert(bbNumPass->getID(ForcerBB));
    }
    return ForceAtLeastOnceCache[BB];
  }

  for (Function::iterator bb = F->begin(); bb != F->end(); ++bb) {
    const PostDominanceFrontier::DomSetType &CDSet = PDF.getFrontier(&*bb);
    std::vector<BasicBlock *> &ForceExecSet = ForceExecCache[&*bb];
    ForceExecSet.insert(ForceExecSet.end(), CDSet.begin(), CDSet.end());

    BasicBlock &entryBlock = F->getEntryBlock();
    if (PDT.properlyDominates(&*bb, &entryBlock)) {
      ForceExecCache[&*bb].push_back(&entryBlock);
      ForceAtLeastOnceCache[BB] = true;
    } else {
      ForceAtLeastOnceCache[BB] = false;
    }
  }

  return findExecForcers(BB, bbNums);
}

void DynamicGiri::findSlice(DynValue &Initial,
                             std::unordered_set<DynValue> &DynSlice,
                             std::set<DynValue *> &DataFlowGraph) {
  Worklist_t Worklist;
  std::unordered_set<DynBasicBlock> processedBBs;

  Worklist.push_back(&Initial);
  ++NumDynSources;

  while (!Worklist.empty()) {
    DynValue *DV = Worklist.front();
    Worklist.pop_front();

    Trace->normalize(*DV);

    std::unordered_set<DynValue>::iterator dvi = DynSlice.find(*DV);
    if (dvi != DynSlice.end()) {
      ++NumDynValsSkipped;
      continue;
    }

    DynSlice.insert(*DV);

    if (DynSlice.size() % 100000 == 0) {
       DEBUG(dbgs() << "100000th Dynamic value processed\n");
       DEBUG(DV->print(dbgs(), lsNumPass));
    }

    DynBasicBlock DBB = DynBasicBlock(*DV);

    if (TraceCD && !DBB.isNull()) {
      BasicBlock &entryBlock = DBB.getParent()->getEntryBlock();
      if (DBB.getBasicBlock() != &entryBlock) {
        if (processedBBs.insert(DBB).second) {
          std::set<unsigned> forcesExecSet;
          bool found = findExecForcers(DBB.getBasicBlock(), forcesExecSet);

          DynBasicBlock Forcer = Trace->getExecForcer(DBB, forcesExecSet);

          if (Forcer.getBasicBlock() == nullptr) {
            errs() << "Could not find Control-dep of this Basic Block \n";
          } else if (Forcer.getBasicBlock() != &entryBlock || !found) {
            DynValue DTerminator = Forcer.getTerminator();
            Trace->addToWorklist(DTerminator, Worklist, *DV);
          }
        }
      }
    }

    Trace->getSourcesFor(*DV, Worklist);
  }

  NumDynValues += DynSlice.size();
  NumLoadsTraced = Trace->totalLoadsTraced;
  NumLoadsLost = Trace->lostLoadsTraced;
}

void DynamicGiri::printBackwardsSlice(const Instruction *Criterion,
                                       std::set<Value *> &Slice,
                                       std::unordered_set<DynValue> &DynSlice,
                                       std::set<DynValue *> &DataFlowGraph) {
  std::error_code EC;
  raw_fd_ostream SliceFile(SliceFilename.c_str(),
                            EC,
                            sys::fs::OF_Append);
  if (EC) {
errs() << "Error opening the slice output file: " << SliceFilename
            << " : " << EC.message() << "\n";
    return;
  }
  SliceFile << "----------------------------------------------------------\n";
  SliceFile << "Static Slice from instruction: \n";
  Criterion->print(SliceFile);
  SliceFile << "\n----------------------------------------------------------\n";
  for (std::set<Value *>::iterator i = Slice.begin(); i != Slice.end(); ++i) {
    Value *V = *i;
    V->print(SliceFile);
    SliceFile << "\n";
    if (Instruction *I = dyn_cast<Instruction>(V))
      SliceFile << "Source Line Info: "
                << SourceLineMappingPass::locateSrcInfo(I)
                << "\n";
  }

  SliceFile << "----------------------------------------------------------\n";
  SliceFile << "Dynamic Slice from instruction: \n";
  Criterion->print(SliceFile);
  SliceFile << "\n----------------------------------------------------------\n";
  for (std::unordered_set<DynValue>::iterator i = DynSlice.begin();
       i != DynSlice.end();
       ++i) {
    DynValue DV = *i;
    DV.print(SliceFile, lsNumPass);
    if (Instruction *I = dyn_cast<Instruction>(i->getValue()))
      SliceFile << "Source Line Info: "
                << SourceLineMappingPass::locateSrcInfo(I)
                << "\n";
  }
  SliceFile << "\n";
  SliceFile.close();
}

void DynamicGiri::getBackwardsSlice(Instruction *I,
                                     std::set<Value *> &Slice,
                                     std::unordered_set<DynValue > &DynSlice,
                                     std::set<DynValue *> &DataFlowGraph) {
  DynValue *DI = Trace->getLastDynValue(I);
  findSlice(*DI, DynSlice, DataFlowGraph);

  std::unordered_set<DynValue>::iterator i = DynSlice.begin();
  while (i != DynSlice.end()) {
    Slice.insert(i->getValue());
    ++i;
  }
}

// New-PM module analysis entry point: the legacy runOnModule body. It performs
// the whole backwards-slice computation for the module (per the -criterion-*
// / default-main-return criterion) and returns *this so that MAM.getResult
// yields this shared DynamicGiri instance.
DynamicGiri DynamicGiri::run(Module &M, ModuleAnalysisManager &MAM) {
  // New-PM: obtain the numbering analyses from the module analysis manager
  // (the legacy getAnalysis<...>()); they are computed once and shared.
  bbNumPass = &MAM.getResult<QueryBBNumbersPass>(M);
  lsNumPass = &MAM.getResult<QueryLSNumbersPass>(M);

  Trace = new TraceFile(TraceFilename, bbNumPass, lsNumPass);

  if (!StartOfSliceLoc.empty()) {
    std::ifstream StartOfSlice(StartOfSliceLoc);
    if (!StartOfSlice.is_open()) {
      errs() << "Error opening criterion file: " << StartOfSliceLoc << "\n";
    }
    std::string StartFilename;
    unsigned StartLoc = 0;
    while (StartOfSlice >> StartFilename >> StartLoc) {
      if (StartLoc == 0) {
        errs() << "Error reading criterion file: " << StartOfSliceLoc << "\n";
        break;
      }
      dbgs() << "Start slicing Filename:Loc is defined as "
             << StartFilename << ":" << StartLoc << "\n";

      Instruction *Criterion = nullptr;
      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
          for (inst_iterator I = inst_begin(&*F); I != inst_end(&*F); ++I) {
          DebugLoc DL = I->getDebugLoc();
            if (DL) {
              StringRef Fn = DL->getFilename();
              unsigned Line = DL->getLine();
              if (Fn.str() == StartFilename && Line == StartLoc) {
                Criterion = &*I;
                DEBUG(dbgs() << "Found instruction matching the LoC: ");
                DEBUG(Criterion->dump());
              }
            }
          }
        }

      if (Criterion != nullptr) {
        std::set<Value *> Slice;
        std::unordered_set<DynValue> DynSlice;
        std::set<DynValue *> DataFlowGraph;
        getBackwardsSlice(Criterion, Slice, DynSlice, DataFlowGraph);
        printBackwardsSlice(Criterion, Slice, DynSlice, DataFlowGraph);
      } else
        errs() << "Didin't find the starting instruction to slice.\n";
      StartLoc = 0;
    }
    StartOfSlice.close();
  } else if (!StartOfSliceInst.empty()) {
    std::ifstream StartOfSlice(StartOfSliceInst);
    if (!StartOfSlice.is_open()) {
      errs() << "Error opening criterion file: " << StartOfSliceInst << "\n";
    }
    std::string StartFunction;
    unsigned StartInst = 0;
    while (StartOfSlice >> StartFunction >> StartInst) {
      if (StartInst == 0) {
        errs() << "Error reading criterion file: " << StartOfSliceInst << "\n";
        break;
      }
      dbgs() << "Start slicing Function:Instruction is defined as "
             << StartFunction << ":" << StartInst << "\n";

      Function *Func = M.getFunction(StartFunction);
      assert(Func);
      Instruction *Criterion = nullptr;
      for (inst_iterator I = inst_begin(Func), E = inst_end(Func); I != E; ++I)
        if (--StartInst == 0) {
          Criterion = &*I;
          DEBUG(dbgs() << "The start of slice instruction is: ");
          DEBUG(Criterion->dump());
          break;
        }

      if (Criterion != nullptr) {
        std::set<Value *> Slice;
        std::unordered_set<DynValue> DynSlice;
        std::set<DynValue *> DataFlowGraph;
        getBackwardsSlice(Criterion, Slice, DynSlice, DataFlowGraph);
        printBackwardsSlice(Criterion, Slice, DynSlice, DataFlowGraph);
      } else
        errs() << "Didin't find the starting instruction to slice.\n";
      StartInst = 0;
    }
    StartOfSlice.close();
  } else {
    Function *Func = M.getFunction("main");
    if (!Func)
      return *this;

    for (inst_iterator I = inst_begin(Func), E = inst_end(Func); I != E; ++I)
      if (isa<ReturnInst>(*I)) {
        DEBUG(dbgs() << "The start of slice instruction is: " << "\n");
        DEBUG(I->dump());
        std::set<Value *> Slice;
        std::unordered_set<DynValue> DynSlice;
        std::set<DynValue *> DataFlowGraph;
        getBackwardsSlice(&*I, Slice, DynSlice, DataFlowGraph);
        printBackwardsSlice(&*I, Slice, DynSlice, DataFlowGraph);
        break;
      }
  }

  return *this;
}
