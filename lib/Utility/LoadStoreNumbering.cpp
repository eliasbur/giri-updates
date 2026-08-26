#include "Utility/LoadStoreNumbering.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace dg;
using namespace llvm;

static cl::opt<bool>
DumpID("dump-lsid", cl::desc("Dump assigned Load/Store ID"), cl::init(false));

#define MAX_PROGRAM_POINTS 2000000

// DEBUG_TYPE is defined after the includes: LLVM 14's dom-tree-builder header
// (GenericDomTreeConstruction.h) does `#undef DEBUG_TYPE`, and the 14.0.0
// DEBUG/STATISTIC machinery references DEBUG_TYPE at the call site.
#define DEBUG_TYPE "giriutil"
#define DEBUG(X) DEBUG_WITH_TYPE(DEBUG_TYPE, X)

AnalysisKey QueryLSNumbersPass::Key;

QueryLoadStoreNumbers QueryLSNumbersPass::run(Module &M,
                                              ModuleAnalysisManager &MAM) {
  DEBUG(dbgs() << "Inside QueryLoadStoreNumbers for module "
               << M.getModuleIdentifier() << "\n");
  QueryLoadStoreNumbers R;
  R.IDMap.clear();
  R.InstMap.clear();
  unsigned count = 0;
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
    for (Function::iterator FI = MI->begin(), FE = MI->end(); FI != FE; ++FI)
      for (BasicBlock::iterator II = FI->begin(), IE = FI->end(); II != IE; ++II) {
        Instruction *I = &*II;
        if (isa<LoadInst>(I) || isa<StoreInst>(I) || isa<SelectInst>(I)) {
          ++count;
          if (DumpID) { I->print(dbgs()); dbgs() << "\n"; }
          R.IDMap[I] = count;
          R.InstMap[count] = I;
        } else if (isa<CallInst>(I)) {
          CallInst *CI = cast<CallInst>(I);
          if (CI->getCalledFunction() && !isTracerFunction(CI->getCalledFunction())) {
            ++count;
            if (DumpID) { I->print(dbgs()); dbgs() << "\n"; }
            R.IDMap[I] = count;
            R.InstMap[count] = I;
          }
        }
      }
  DEBUG(dbgs() << "Number of monitored program points: " << count << "\n");
  if (count > MAX_PROGRAM_POINTS)
    errs() << "Number of monitored program points exceeds maximum value.\n";
  return R;
}
