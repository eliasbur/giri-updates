#define DEBUG_TYPE "giriutil"

#include "Utility/SourceLineMapping.h"
#include "Utility/Utils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/CallBase.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/InstIterator.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace llvm;
using namespace dg;

static cl::opt<std::string>
FunctionName("mapping-function",
              cl::desc("The function name to be mapped"),
              cl::init(""));

static cl::opt<std::string>
MappingFileName("mapping-output",
                  cl::desc("The output filename of the source line mapping"),
                  cl::init("-"));

STATISTIC(NumFoundSrc, "Number of source information locations found");
STATISTIC(NumNotFoundSrc, "Number of source information locations not found");
STATISTIC(NumQueriedSrc, "Number of queried including ignored LLVM insts");

char SourceLineMappingPass::ID = 0;

static RegisterPass<dg::SourceLineMappingPass>
X("srcline-mapping", "Mapping LLVM inst to source line number");

std::string SourceLineMappingPass::locateSrcInfo(Instruction *I) {
  ++NumQueriedSrc;

  DebugLoc DL = I->getDebugLoc();
  if (DL) {
    ++NumFoundSrc;
    StringRef Fn = DL->getFilename();
    if (Fn == "no-file")
      return "";
    StringRef Dir = DL->getDirectory();
    unsigned Line = DL->getLine();
    std::stringstream ss;
    ss << Dir.str() << "/" << Fn.str() << ":" << Line;
    return ss.str();
  }
  if (isa<PHINode>(I) || isa<AllocaInst>(I) || isa<BranchInst>(I))
    return "";
  else if (CallInst *CI = dyn_cast<CallInst>(I)) {
    Function *CalledFunc = CI->getCalledFunction();
    if (CalledFunc) {
      if (isTracerFunction(CalledFunc))
        return "";
      std::string fnname = CalledFunc->getName().str();
      if (fnname.compare(0, 9, "llvm.dbg.") == 0)
        return "";
    }
  } else if (InvokeInst *II = dyn_cast<InvokeInst>(I)) {
    if (Value *CalledVal = II->getCalledValue()) {
      if (Function *CalledFunc = dyn_cast<Function>(CalledVal->stripPointerCasts())) {
        if (isTracerFunction(CalledFunc))
          return "";
        std::string fnname = CalledFunc->getName().str();
        if (fnname.compare(0, 9, "llvm.dbg.") == 0)
          return "";
      }
    }
  }
  NumNotFoundSrc++;
  return "NIL";
}

void SourceLineMappingPass::mapCompleteFile(Module &M, raw_ostream &Output) {
  for (Module::iterator F = M.begin(); F != M.end(); ++F)
    if (!F->isDeclaration() && F->hasName())
      mapOneFunction(&*F, Output);
}

void SourceLineMappingPass::mapOneFunction(Function *F, raw_ostream &Output) {
  assert(F && !F->isDeclaration() && F->hasName());
  Output << "========================================================\n";
  Output << "Source line mapping for function: " << F->getName() << "\n";
  Output << "========================================================\n";

  int instCount = 0;
  for (inst_iterator I = inst_begin(F); I != inst_end(F); ++I) {
    Output << ++instCount << ": ";
    I->print(Output);
    Output << ": ";
    Output << locateSrcInfo(&*I);
    Output << "\n";
  }
}

bool SourceLineMappingPass::runOnModule(Module &M) {
  std::error_code EC;
  raw_fd_ostream MappingFile(MappingFileName.c_str(), EC, sys::fs::F_None);

  if (!FunctionName.empty())
    mapOneFunction(M.getFunction(FunctionName), MappingFile);
  else
    mapCompleteFile(M, MappingFile);

  return false;
}