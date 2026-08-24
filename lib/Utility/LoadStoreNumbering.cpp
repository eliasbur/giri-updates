#define DEBUG_TYPE "giriutil"

#define MAX_PROGRAM_POINTS 2000000

#include "Utility/LoadStoreNumbering.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
// LLVM 8 removed the bare DEBUG(X) macro that LLVM 3.4's Debug.h provided;
// DEBUG_TYPE is set above, so map it to the 8.0.0 DEBUG_WITH_TYPE.
#define DEBUG(X) DEBUG_WITH_TYPE(DEBUG_TYPE, X)
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace dg;
using namespace llvm;

static cl::opt<bool>
DumpID("dump-lsid", cl::desc("Dump assigned Load/Store ID"), cl::init(false));

char LoadStoreNumberPass::ID    = 0;
char QueryLoadStoreNumbers::ID  = 0;
char RemoveLoadStoreNumbers::ID = 0;

static RegisterPass<LoadStoreNumberPass>
X ("lsnum", "Assign Unique Identifiers to Loads and Stores");

static RegisterPass<RemoveLoadStoreNumbers>
Z ("remove-lsnum", "Remove Unique Identifiers of Loads and Stores");

static RegisterPass<QueryLoadStoreNumbers>
Y ("query-lsnum", "Query Unique Identifiers of Loads and Stores");

bool LoadStoreNumberPass::runOnModule(Module &M) {
  return false;
}

bool QueryLoadStoreNumbers::runOnModule(Module &M) {
  DEBUG(dbgs() << "Inside QueryLoadStoreNumbers for module "
                << M.getModuleIdentifier() << "\n");
  IDMap.clear();
  InstMap.clear();
  unsigned count = 0;
  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
    for (Function::iterator FI = MI->begin(), FE = MI->end(); FI != FE; ++FI)
      for (BasicBlock::iterator II = FI->begin(), IE = FI->end(); II != IE; ++II) {
        Instruction *I = &*II;
        if (isa<LoadInst>(I) || isa<StoreInst>(I) || isa<SelectInst>(I)) {
          ++count;
          if (DumpID) { I->print(dbgs()); dbgs() << "\n"; }
          IDMap[I] = count;
          InstMap[count] = I;
        } else if (isa<CallInst>(I)) {
          CallInst *CI = cast<CallInst>(I);
          if (CI->getCalledFunction() && !isTracerFunction(CI->getCalledFunction())) {
            ++count;
            if (DumpID) { I->print(dbgs()); dbgs() << "\n"; }
            IDMap[I] = count;
            InstMap[count] = I;
          }
        }
      }
  DEBUG(dbgs() << "Number of monitored program points: " << count << "\n");
  if (count > MAX_PROGRAM_POINTS)
    errs() << "Number of monitored program points exceeds maximum value.\n";
  return false;
}

bool RemoveLoadStoreNumbers::runOnModule(Module &M) {
  return false;
}