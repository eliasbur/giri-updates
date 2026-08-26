//===- CountSrcLines.h - Dynamic Slicing Pass -------------------*- C++ -*-===//
//
//                      Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files defines passes that are used for dynamic slicing.
//
// New pass manager port (LLVM 14.0.0): CountSrcLines is now a new-PM module
// pass. It takes the QueryBBNumbers/QueryLSNumbers analyses from the module
// analysis manager (replacing the legacy getAnalysis<...>()).
//
//===----------------------------------------------------------------------===//

#ifndef DG_COUNTSRCLINES_H
#define DG_COUNTSRCLINES_H

#include "Giri/TraceFile.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"

#include "llvm/IR/PassManager.h"
#include "llvm/IR/InstVisitor.h"

#include <deque>
#include <set>
#include <unordered_set>

using namespace llvm;

namespace dg {

/// \class This pass counts the number of static Source lines/LLVM insts.
/// executed in a trace.
class CountSrcLines : public PassInfoMixin<CountSrcLines> {
public:
  /// Entry point for this new-PM pass. Using trace information, find the
  /// static number of source lines and LLVM instructions in a trace.
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

  StringRef getPassName() const {
    return "Count static #SourceLines/LLVM Insts in a trace";
  }

  void countLines(const std::string &bbrecord_file);

  std::unordered_set<unsigned> readBB(const std::string &bbrecord_file);

private:
  const QueryBasicBlockNumbers *bbNumPass;
  const QueryLoadStoreNumbers  *lsNumPass;

};

} // END namespace dg

#endif
