//===- BasicBlockNumbering.h - Provide BB identifiers -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class provides LLVM passes that provide a *stable* numbering of basic
// blocks that does not depend on their address in memory (which is
// nondeterministic).
//
//===----------------------------------------------------------------------===//

#ifndef DG_BASICBLOCKNUMBERING_H
#define DG_BASICBLOCKNUMBERING_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <map>

using namespace llvm;

namespace dg {

class BasicBlockNumberPass : public ModulePass {
public:
  static char ID;
  BasicBlockNumberPass () : ModulePass (ID) {}

  virtual bool runOnModule (Module & M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };
};

class QueryBasicBlockNumbers : public ModulePass {
public:
  static char ID;

  QueryBasicBlockNumbers () : ModulePass (ID) {}

  virtual bool runOnModule (Module & M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };

  unsigned getID (BasicBlock *BB) const {
    std::map<BasicBlock*, unsigned>::const_iterator I = IDMap.find(BB);
    if (I == IDMap.end())
      return 0;
    return I->second;
  }

  BasicBlock * getBlock (unsigned id) const {
    std::map<unsigned, BasicBlock *>::const_iterator i = BBMap.find (id);
    if (i != BBMap.end())
      return i->second;
    return 0;
  }

protected:
  std::map<BasicBlock*, unsigned> IDMap;
  std::map<unsigned, BasicBlock *> BBMap;
};

class RemoveBasicBlockNumbers : public ModulePass {
public:
  static char ID;

  RemoveBasicBlockNumbers () : ModulePass (ID) {}

  virtual bool runOnModule (Module & M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  };
};

} // END namespace dg

#endif