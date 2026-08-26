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
// New pass manager port (LLVM 14.0.0): mirrors BasicBlockNumbering.h —
// LoadStoreNumberPass / RemoveLoadStoreNumbers are no-op new-PM module
// passes; QueryLoadStoreNumbers is the analysis result (getID/getInstByID);
// QueryLSNumbersPass is the new-PM module analysis that computes the
// numbering (byte-identical to the legacy runOnModule).
//
//===----------------------------------------------------------------------===//

#ifndef DG_LOADSTORENUMBERING_H
#define DG_LOADSTORENUMBERING_H

#include "Utility/Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/InstVisitor.h"

#include <map>
#include <unordered_map>

using namespace llvm;

namespace dg {

/// The stable load/store/call/select numbering: the ID maps plus the same
/// getID/getInstByID accessors the legacy QueryLoadStoreNumbers exposed.
class QueryLoadStoreNumbers {
public:
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

  std::map<Instruction*, unsigned> IDMap;
  std::map<unsigned, Instruction *> InstMap;
};

/// New-PM module analysis that computes the stable load/store numbering.
/// Logic is byte-identical to the legacy QueryLoadStoreNumbers::runOnModule.
class QueryLSNumbersPass : public AnalysisInfoMixin<QueryLSNumbersPass> {
  friend AnalysisInfoMixin<QueryLSNumbersPass>;

  static AnalysisKey Key;

public:
  using Result = QueryLoadStoreNumbers;

  Result run(Module &M, ModuleAnalysisManager &MAM);
};

/// New-PM no-op module pass (legacy -lsnum). Forces the numbering analysis
/// so the -dump-lsid debug output (and any later consumer) see the IDs.
class LoadStoreNumberPass : public PassInfoMixin<LoadStoreNumberPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    MAM.getResult<QueryLSNumbersPass>(M);
    return PreservedAnalyses::all();
  }
};

/// New-PM no-op module pass (legacy -remove-lsnum).
class RemoveLoadStoreNumbers : public PassInfoMixin<RemoveLoadStoreNumbers> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    return PreservedAnalyses::all();
  }
};

} // END namespace dg

#endif
