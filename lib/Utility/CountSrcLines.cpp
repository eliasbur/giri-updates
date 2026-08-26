//===- CountSrcLines.cpp - Find dynamic backwards slice analysis pass --------//
//
//                      Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements an analysis pass that allows clients to find the
// instructions contained within the dynamic backwards slice of a specified
// instruction.
//
//===----------------------------------------------------------------------===//


#include "Utility/CountSrcLines.h"
#include "Utility/SourceLineMapping.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace llvm;
using namespace dg;
using namespace std;

//===----------------------------------------------------------------------===//
//                            Pass Statistics
//===----------------------------------------------------------------------===//
// DEBUG_TYPE is defined after the includes: LLVM 14's dom-tree-builder header
// (GenericDomTreeConstruction.h) does `#undef DEBUG_TYPE`, and the 14.0.0 STATISTIC
// macro references DEBUG_TYPE at the call site.
#define DEBUG_TYPE "giriutil"
STATISTIC(NumOfDynamicBBs, "Number of basic blocks executed in trace");
STATISTIC(NumSrcLines, "Number of static source lines executed in trace");
STATISTIC(NumSrcLinesMissing, "Number of insts missing debug info");
STATISTIC(NumWithoutSrcLines, "Number of ignored insts without source lines");
STATISTIC(NumStaticInst, "Number of static LLVM instructions executed");
STATISTIC(NumBBsNoSrc, "Number of BBs whose source line debug info is missing");

//===----------------------------------------------------------------------===//
//                        Command Line Arguments.
//===----------------------------------------------------------------------===//
cl::opt<string> TraceFilename("trace-file",
                              cl::desc("Trace filename"),
                              cl::init("bbrecord"));

//===----------------------------------------------------------------------===//
//                        CountSrcLines Pass Implementations
//===----------------------------------------------------------------------===//

void CountSrcLines::countLines(const string &bbrecord_file) {
  unordered_set<unsigned> bb_set = readBB(bbrecord_file);
  set<string> srcLines;
  set<string> BBsNoSrc;

  unordered_set<unsigned>::iterator curr;
  for (curr = bb_set.begin(); curr != bb_set.end(); ++curr) {
     BasicBlock *BB = bbNumPass->getBlock(*curr);
     NumStaticInst += BB->size();
     for (BasicBlock::iterator it = BB->begin(); it != BB->end(); ++it) {
       string srcLineInfo = SourceLineMappingPass::locateSrcInfo(&*it);
       if (srcLineInfo.empty()) // Ignored instructions without source lines
         NumWithoutSrcLines++;
       else if (srcLineInfo == "NIL") { // No source line found using debug info
         NumSrcLinesMissing++;
         BBsNoSrc.insert(srcLineInfo);
       } else
         srcLines.insert(srcLineInfo);
     }
  }

  NumSrcLines = srcLines.size();
  NumBBsNoSrc = BBsNoSrc.size();
}

unordered_set<unsigned> CountSrcLines::readBB(const string &bbrecord) {
  int bb_fd = open(bbrecord.c_str(), O_RDONLY);
  if (!bb_fd)
     report_fatal_error(Twine("Error opening trace file: ") + bbrecord + "!\n");

  unordered_set<unsigned> bb_set; // Keep track of basic bock ID
  Entry entry;
  while (read(bb_fd, &entry, sizeof(entry)) == sizeof(entry)) {
    if (entry.type == RecordType::BBType) {
      bb_set.insert(entry.id);
      ++NumOfDynamicBBs;
    }
    if (entry.type == RecordType::ENType)
      break;
  }

  close(bb_fd);

  return bb_set;
}

PreservedAnalyses CountSrcLines::run(Module &M, ModuleAnalysisManager &MAM) {
  // Get references to the analyses used by this pass (the new-PM equivalent
  // of the legacy getAnalysis<...>(); they are computed once and shared).
  bbNumPass = &MAM.getResult<QueryBBNumbersPass>(M);
  lsNumPass = &MAM.getResult<QueryLSNumbersPass>(M);

  // Open the trace file and read the basic block entries.
  countLines(TraceFilename);

  // This is an analysis pass, so nothing is modified.
  return PreservedAnalyses::all();
}
