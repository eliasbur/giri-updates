//===- LoadStoreNumbering.h - Provide load/store identifiers ----*- C++ -*-===//
//
//                     Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides LLVM passes that provide a *stable* numbering of load
// and store instructions that does not depend on their address in memory
// (which is nondeterministic).
//
//===----------------------------------------------------------------------===//

#ifndef DG_LOADSTORENUMBERING_H
#define DG_LOADSTORENUMBERING_H

#include "Utility/Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/InstVisitor.h"

#include <map>
#include <unordered_map>

using namespace llvm;

namespace dg {

class LoadStoreNumberPass : public ModulePass {
public:
  static char ID;
  LoadStoreNumberPass() : ModulePass(ID), count(0) {}

  virtual bool runOnModule(Module &M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };

private:
  unsigned count;
};

class QueryLoadStoreNumbers : public ModulePass {
public:
  static char ID;
  QueryLoadStoreNumbers() : ModulePass(ID) {}

  virtual bool runOnModule(Module & M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };

  unsigned getID(const Instruction *I) const {
    std::map<Instruction*, unsigned>::const_iterator im = IDMap.find(const_cast<Instruction*>(I));
    if (im != IDMap.end())
      return im->second;
    return 0;
  }

  Instruction *getInstByID(unsigned id) const {
    std::map<unsigned, Instruction *>::const_iterator im = InstMap.find(id);
    if (im != InstMap.end())
      return im->second;
    return 0;
  }

protected:
  std::map<Instruction*, unsigned> IDMap;
  std::map<unsigned, Instruction *> InstMap;
};

class RemoveLoadStoreNumbers : public ModulePass {
public:
  static char ID;
  RemoveLoadStoreNumbers() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };
};

} // END namespace dg

#endif