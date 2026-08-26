//=- llvm/Analysis/PostDominanceFrontier.h - Post Dominance Frontier Calculation-*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file exposes interfaces to post dominance frontier information.
//
// New pass manager port (LLVM 14.0.0): the legacy FunctionPass (which required
// PostDominatorTreeWrapperPass) is split into:
//  - PostDominanceFrontier: a plain class holding the frontier map and the
//    getFrontier/computeFrontiers accessors (the slicing logic is unchanged).
//    DynamicGiri computes it inline from a PostDominatorTree (constructed
//    directly with its public Function& ctor), replacing the legacy
//    new PostDominatorTreeWrapperPass lazy-wrapper.
//  - PostDominanceFrontierAnalysis: a new-PM function analysis (result is a
//    PostDominanceFrontier) that takes the built-in PostDominatorTree analysis.
//  - PostDominanceFrontierPass: a function pass exposed under the pipeline
//    name "postdomfrontier" (runs the analysis).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H
#define LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H

#include <map>
#include <set>
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/PostDominators.h"

namespace llvm {

/// Post-dominance frontier for a function. Plain data + computation; not a
/// pass (the new-PM function analysis below computes and returns one).
class PostDominanceFrontier {
public:
  typedef std::set<BasicBlock*> DomSetType;
  typedef std::map<BasicBlock*, DomSetType> DomSetMap;

  const DomSetType &getFrontier(BasicBlock *BB) const {
    static const DomSetType emptySet;
    DomSetMap::const_iterator I = Frontiers.find(BB);
    return I != Frontiers.end() ? I->second : emptySet;
  }

  void computeFrontiers(const PostDominatorTree &DT) {
    Frontiers.clear();
    if (const DomTreeNode *Root = DT.getRootNode())
      calculate(DT, Root);
  }

private:
  DomSetMap Frontiers;

  const DomSetType &calculate(const PostDominatorTree &DT, const DomTreeNode *Node);
};

/// New-PM function analysis: computes a PostDominanceFrontier for a function
/// from the built-in PostDominatorTree analysis.
class PostDominanceFrontierAnalysis
    : public AnalysisInfoMixin<PostDominanceFrontierAnalysis> {
  friend AnalysisInfoMixin<PostDominanceFrontierAnalysis>;

  static AnalysisKey Key;

public:
  using Result = PostDominanceFrontier;

  Result run(Function &F, FunctionAnalysisManager &FAM);
};

/// Function pass exposed as the "postdomfrontier" pipeline name.
class PostDominanceFrontierPass
    : public PassInfoMixin<PostDominanceFrontierPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    FAM.getResult<PostDominanceFrontierAnalysis>(F);
    return PreservedAnalyses::all();
  }
};

} // End llvm namespace

#endif
