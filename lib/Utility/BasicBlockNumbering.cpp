//===- BasicBlockNumbering.cpp - Provide BB identifiers ---------*- C++ -*-===//
//
//                    Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the analysis that assigns a unique ID to each basic
// block.
//
//===----------------------------------------------------------------------===//

#include "Utility/BasicBlockNumbering.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace dg;
using namespace llvm;

static cl::opt<bool>
DumpID("dump-bbid", cl::desc("Dump assigned basic block ID"), cl::init(false));

// LLVM 8 removed the bare DEBUG(X) macro that LLVM 3.4's Debug.h provided;
// DEBUG_TYPE is set below (after the includes, because LLVM 14's
// GenericDomTreeConstruction.h #undef DEBUG_TYPE), so map it to
// DEBUG_WITH_TYPE.
#define DEBUG_TYPE "giriutil"
#define DEBUG(X) DEBUG_WITH_TYPE(DEBUG_TYPE, X)

AnalysisKey QueryBBNumbersPass::Key;

QueryBasicBlockNumbers QueryBBNumbersPass::run(Module &M,
                                               ModuleAnalysisManager &MAM) {
  DEBUG(dbgs() << "Inside QueryBasicBlockNumbers for module "
               << M.getModuleIdentifier()
               << "\n");

  QueryBasicBlockNumbers R;
  R.IDMap.clear();
  R.BBMap.clear();

  unsigned count = 0;
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
    for (Function::iterator BB = MI->begin(), BE = MI->end(); BB != BE; ++BB) {
      ++count;
      BasicBlock *block = &*BB;
      if (DumpID)
        dbgs() << count << " : " << block->getName() << "\n";
      R.IDMap[block] = count;
      R.BBMap[count] = block;
    }
  DEBUG(dbgs() << "Total Number of Basic Blocks: " << count << "\n");

  return R;
}
