//===- UtilityPassPlugin.cpp - new-PM pass/analysis registration for libdgutility
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// New pass manager port (LLVM 14.0.0): registers the Utility-layer passes and
// analyses with the new pass manager, replacing the legacy
// static RegisterPass<...> initializers.
//
// opt loads this shared library twice per invocation:
//   -load libdgutility.so               (legacy-style dlopen at startup, so
//                                        the cl::opt globals in this library
//                                        are registered before cl parsing)
//   -load-pass-plugin=libdgutility.so   (new-PM dlopen after cl parsing, which
//                                        calls llvmGetPassPluginInfo below to
//                                        register the pipeline names)
//
// The pass/analysis classes and their behavior live in the Utility headers
// and their .cpp files; this file only wires the -passes pipeline names to
// them.
//
//===----------------------------------------------------------------------===//

#include "Utility/BasicBlockNumbering.h"
#include "Utility/CountSrcLines.h"
#include "Utility/LoadStoreNumbering.h"
#include "Utility/PostDominanceFrontier.h"
#include "Utility/SourceLineMapping.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;
using namespace dg;

// clang-format off
extern "C" LLVM_EXTERNAL_VISIBILITY
PassPluginLibraryInfo llvmGetPassPluginInfo() {
  static PassPluginLibraryInfo Info = {
    LLVM_PLUGIN_API_VERSION,
    "Giri Utility passes (new pass manager)",
    "1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
          [](StringRef Name, FunctionPassManager &FPM,
             ArrayRef<PassBuilder::PipelineElement>) -> bool {
            if (Name == "postdomfrontier") {
              FPM.addPass(PostDominanceFrontierPass());
              return true;
            }
            return false;
          });
      PB.registerPipelineParsingCallback(
          [](StringRef Name, ModulePassManager &MPM,
             ArrayRef<PassBuilder::PipelineElement>) -> bool {
            if (Name == "bbnum") {
              MPM.addPass(BasicBlockNumberPass());
              return true;
            }
            if (Name == "remove-bbnum") {
              MPM.addPass(RemoveBasicBlockNumbers());
              return true;
            }
            if (Name == "lsnum") {
              MPM.addPass(LoadStoreNumberPass());
              return true;
            }
            if (Name == "remove-lsnum") {
              MPM.addPass(RemoveLoadStoreNumbers());
              return true;
            }
            if (Name == "countsrc") {
              MPM.addPass(CountSrcLines());
              return true;
            }
            if (Name == "srcline-mapping") {
              MPM.addPass(SourceLineMappingPass());
              return true;
            }
            return false;
          });
      // Register the Utility module analyses (consumed by the Giri passes and
      // the bbnum/lsnum passes). PostDominanceFrontierAnalysis is a function
      // analysis consumed by DynamicGiri (in libgiri) via FAM.getResult; it is
      // also registered here so both plugins see one shared registration.
      PB.registerAnalysisRegistrationCallback(
          [](ModuleAnalysisManager &MAM) {
            MAM.registerPass([] { return QueryBBNumbersPass(); });
            MAM.registerPass([] { return QueryLSNumbersPass(); });
          });
      PB.registerAnalysisRegistrationCallback(
          [](FunctionAnalysisManager &FAM) {
            FAM.registerPass([] { return PostDominanceFrontierAnalysis(); });
          });
    }};
  return Info;
}
// clang-format on
