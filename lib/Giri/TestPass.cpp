//===- TestPass.cpp - Test the giri pass -----------------------------------===//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass is used to test the giri pass.
//
// New pass manager port (LLVM 14.0.0): TestGiri is a new-PM module pass (the
// legacy -test-giri). It obtains the shared DynamicGiri module analysis via
// MAM.getResult<DynamicGiri>(M) (the legacy getAnalysis<DynamicGiri>()).
// Registered under the "test-giri" pipeline name in GiriPassPlugin.cpp.
//
//===----------------------------------------------------------------------===//

#include "Giri/Giri.h"

#include "llvm/Support/CommandLine.h"

using namespace llvm;

// Command line options
static cl::opt<unsigned>
InstIndex("inst-index", cl::desc("Instruction index in function"), cl::init(0));

static cl::opt<std::string>
Funcname ("funcname", cl::desc("Function Name"), cl::init("main"));

namespace giri {

/// Entry point for this pass. Find the instruction specified by the user
/// and find the backwards slice of it.
PreservedAnalyses TestGiri::run(Module &M, ModuleAnalysisManager &MAM) {
  // Get a reference to the function specified by the user.
  Function *F = M.getFunction(Funcname);
  if (!F) return PreservedAnalyses::all();

  // Find the instruction referenced by the user and get its backwards slice.
  unsigned index = 0;
  DynamicGiri &Giri = MAM.getResult<DynamicGiri>(M);
  for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
    for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
      if (index++ == InstIndex) {
        std::cerr << "Trace for: ";
        I->print(llvm::errs());
        std::cerr << std::endl;
        Giri.getBackwardsSlice(&*I, mySliceOfLife, myDynSliceOfLife,
                               myDataFlowGraph);
        break;
      }
    }
  }

  // We never modify the module.
  return PreservedAnalyses::all();
}

} // END namespace giri
