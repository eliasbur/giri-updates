//===- PostDominanceFrontier.cpp - Post-Dominance frontier Calculation --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the post-dominance frontier construction algorithms.
//
// New pass manager port (LLVM 14.0.0): the frontier computation is unchanged;
// it is computed from a PostDominatorTree (previously obtained from the
// PostDominatorTreeWrapperPass).
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giriutil"

#include "Utility/PostDominanceFrontier.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SetOperations.h"

using namespace llvm;

AnalysisKey PostDominanceFrontierAnalysis::Key;

const PostDominanceFrontier::DomSetType&
PostDominanceFrontier::calculate(const PostDominatorTree &DT,
                                 const DomTreeNode *Node) {
  BasicBlock *BB = Node->getBlock();
  DomSetType &S = Frontiers[BB];

  if (BB) {
    for (pred_iterator SI = pred_begin(BB), SE = pred_end(BB); SI != SE; ++SI) {
      BasicBlock *P = *SI;
      DomTreeNode *SINode = DT.getNode(P);
      if (SINode && SINode->getIDom() != Node)
        S.insert(P);
    }
  }

  for (DomTreeNode::const_iterator NI = Node->begin(), NE = Node->end(); NI != NE; ++NI) {
    DomTreeNode *IDominee = *NI;
    const DomSetType &ChildDF = calculate(DT, IDominee);

    for (DomSetType::const_iterator CDFI = ChildDF.begin(), CDFE = ChildDF.end();
         CDFI != CDFE; ++CDFI) {
      if (!DT.properlyDominates(Node, DT.getNode(*CDFI)))
        S.insert(*CDFI);
    }
  }

  return S;
}

// New-PM function analysis: take the built-in PostDominatorTree analysis and
// compute the frontier from it (the legacy pass took it from
// PostDominatorTreeWrapperPass::getPostDomTree()).
PostDominanceFrontier
PostDominanceFrontierAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  // PostDominatorTreeAnalysis is the new-PM analysis whose Result is a
  // PostDominatorTree; PostDominatorTree itself is just the data structure.
  const PostDominatorTree &PDT = FAM.getResult<PostDominatorTreeAnalysis>(F);

  PostDominanceFrontier PDF;
  if (PDT.getRootNode())
    PDF.computeFrontiers(PDT);
  return PDF;
}
