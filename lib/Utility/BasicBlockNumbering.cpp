//===- BasicBlockNumbering.cpp - Provide BB identifiers ---------*- C++ -*-===//
//
//                    Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that assigns a unique ID to each basic block.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giriutil"

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

char BasicBlockNumberPass::ID    = 0;
char QueryBasicBlockNumbers::ID  = 0;
char RemoveBasicBlockNumbers::ID = 0;

static RegisterPass<dg::BasicBlockNumberPass>
X("bbnum", "Assign Unique Identifiers to Basic Blocks");

static RegisterPass<dg::QueryBasicBlockNumbers>
Y("query-bbnum", "Query Unique Identifiers of Basic Blocks");

static RegisterPass<dg::RemoveBasicBlockNumbers>
Z("remove-bbnum", "Remove Unique Identifiers of Basic Blocks");

bool BasicBlockNumberPass::runOnModule(Module &M) {
  return false;
}

bool QueryBasicBlockNumbers::runOnModule(Module &M) {
  DEBUG(dbgs() << "Inside QueryBasicBlockNumbers for module "
                << M.getModuleIdentifier()
                << "\n");

  IDMap.clear();
  BBMap.clear();

  unsigned count = 0;
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
    for (Function::iterator BB = MI->begin(), BE = MI->end(); BB != BE; ++BB) {
      ++count;
      BasicBlock *block = &*BB;
      if (DumpID)
        dbgs() << count << " : " << block->getName() << "\n";
      IDMap[block] = count;
      BBMap[count] = block;
    }
  DEBUG(dbgs() << "Total Number of Basic Blocks: " << count << "\n");

  return false;
}

bool RemoveBasicBlockNumbers::runOnModule(Module &M) {
  return false;
}