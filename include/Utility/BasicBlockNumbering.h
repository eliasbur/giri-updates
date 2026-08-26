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
// New pass manager port (LLVM 14.0.0):
//  - BasicBlockNumberPass / RemoveBasicBlockNumbers: new-PM *module passes*
//    that are pure no-ops (as they were under the legacy PM; the IDs never
//    live in the IR). They exist so the -passes pipeline shape is unchanged.
//  - QueryBasicBlockNumbers: the analysis *result* (the ID maps + getID/
//    getBlock accessors). Consumers obtain one shared instance via
//    MAM.getResult<QueryBBNumbersPass>(M), the new-PM equivalent of the
//    legacy getAnalysis<QueryBasicBlockNumbers>().
//  - QueryBBNumbersPass: the new-PM *module analysis* whose run() computes
//    the numbering (byte-identical to the legacy runOnModule).
//
//===----------------------------------------------------------------------===//

#ifndef DG_BASICBLOCKNUMBERING_H
#define DG_BASICBLOCKNUMBERING_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

#include <map>

using namespace llvm;

namespace dg {

/// The stable basic-block numbering: the ID maps plus the same getID/getBlock
/// accessors the legacy QueryBasicBlockNumbers pass exposed.
class QueryBasicBlockNumbers {
public:
  unsigned getID(BasicBlock *BB) const {
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

  std::map<BasicBlock*, unsigned> IDMap;
  std::map<unsigned, BasicBlock *> BBMap;
};

/// New-PM module analysis that computes the stable basic-block numbering.
/// Logic is byte-identical to the legacy QueryBasicBlockNumbers::runOnModule.
class QueryBBNumbersPass : public AnalysisInfoMixin<QueryBBNumbersPass> {
  friend AnalysisInfoMixin<QueryBBNumbersPass>;

  static AnalysisKey Key;

public:
  using Result = QueryBasicBlockNumbers;

  Result run(Module &M, ModuleAnalysisManager &MAM);
};

/// New-PM no-op module pass (legacy -bbnum; the IDs never live in the IR).
/// It forces the numbering analysis so the -dump-bbid debug output (and any
/// later consumer) see the IDs, matching the legacy getAnalysis-triggered
/// behavior.
class BasicBlockNumberPass : public PassInfoMixin<BasicBlockNumberPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    MAM.getResult<QueryBBNumbersPass>(M);
    return PreservedAnalyses::all();
  }
};

/// New-PM no-op module pass (legacy -remove-bbnum).
class RemoveBasicBlockNumbers : public PassInfoMixin<RemoveBasicBlockNumbers> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    return PreservedAnalyses::all();
  }
};

} // END namespace dg

#endif
