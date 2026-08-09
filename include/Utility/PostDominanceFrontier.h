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
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H
#define LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H

#include <set>
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {

struct PostDominanceFrontier : public FunctionPass {
  typedef FunctionPass SuperClass;
  typedef std::set<BasicBlock*> DomSetType;
  typedef DenseMap<BasicBlock*, DomSetType> DomSetMap;

  static char ID;
  PostDominanceFrontier() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) override {
    Frontiers.clear();
    PostDominatorTreeWrapperPass *PDP = getAnalysisIfAvailable<PostDominatorTreeWrapperPass>();
    if (!PDP) return false;
    PostDominatorTree &DT = PDP->getPostDomTree();
    if (const DomTreeNode *Root = DT.getRootNode())
      calculate(DT, Root);
    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<PostDominatorTreeWrapperPass>();
  }

  const DomSetType &getFrontier(BasicBlock *BB) const {
    static const DomSetType emptySet;
    DomSetMap::const_iterator I = Frontiers.find(BB);
    return I != Frontiers.end() ? I->second : emptySet;
  }

  static FunctionPass *createPostDomFrontier() {
    return new PostDominanceFrontier();
  }

private:
  DomSetMap Frontiers;

  const DomSetType &calculate(const PostDominatorTree &DT, const DomTreeNode *Node);
};

FunctionPass* createPostDomFrontier();

} // End llvm namespace

#endif