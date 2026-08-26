//===- GiriPassPlugin.cpp - new-PM pass/analysis registration for libgiri
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// New pass manager port (LLVM 14.0.0): registers the Giri passes and
// analyses with the new pass manager, replacing the legacy
// static RegisterPass<...> initializers.
//
// opt loads this shared library twice per invocation (see
// UtilityPassPlugin.cpp for why):
//   -load libgiri.so                     (registers the cl::opt globals such as
//                                         -trace-file/-slice-file/-criterion-*)
//   -load-pass-plugin=libgiri.so        (calls llvmGetPassPluginInfo below)
//
//===----------------------------------------------------------------------===//

#include "Giri/Giri.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;
using namespace giri;

// clang-format off
extern "C" LLVM_EXTERNAL_VISIBILITY
PassPluginLibraryInfo llvmGetPassPluginInfo() {
  static PassPluginLibraryInfo Info = {
    LLVM_PLUGIN_API_VERSION,
    "Giri passes (new pass manager)",
    "1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
          [](StringRef Name, ModulePassManager &MPM,
             ArrayRef<PassBuilder::PipelineElement>) -> bool {
            if (Name == "trace-giri") {
              MPM.addPass(TracingNoGiri());
              return true;
            }
            if (Name == "dgiri") {
              MPM.addPass(DynamicGiriPass());
              return true;
            }
            if (Name == "test-giri") {
              MPM.addPass(TestGiri());
              return true;
            }
            return false;
          });
      // Register the Giri module analysis (DynamicGiri) so the "dgiri"/
      // "test-giri" wrappers can obtain the shared instance via
      // MAM.getResult<DynamicGiri>(M). Also (re)register the Utility numbering
      // analyses: MAM.registerPass is a no-op if the type is already
      // registered (e.g. by the Utility plugin), but it keeps libgiri usable
      // when it is loaded as a pass plugin on its own.
      PB.registerAnalysisRegistrationCallback(
          [](ModuleAnalysisManager &MAM) {
            MAM.registerPass([] { return DynamicGiri(); });
            MAM.registerPass([] { return QueryBBNumbersPass(); });
            MAM.registerPass([] { return QueryLSNumbersPass(); });
          });
    }};
  return Info;
}
// clang-format on
