//===-- tracer - Tracing for Dynamic Slicing Tool -------------------------===//
//
//                      Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed
// under the University of Illinois Open Source License. See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
//
// This program is a tool to run the dynamic tracing instrumentation on a
// bitcode file.  We do this because *some* platforms (cough *Cygwin* cough)
// don't support LLVM's shared libraries.
//
// New pass manager port (LLVM 14.0.0): the pipeline is built programmatically
// with the new ModulePassManager (it links libgiri/libdgutility directly, so
// no -load / -load-pass-plugin plugin loading is needed; the analyses are
// registered on the ModuleAnalysisManager in code).
//
//===----------------------------------------------------------------------===//

#include "Giri/Giri.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Signals.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#include <fstream>
#include <iostream>
#include <memory>

using namespace llvm;
using namespace dg;
using namespace giri;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bytecode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<bool>
Force("f", cl::desc("Overwrite output files"));

static cl::opt<bool>
DoTrace("trace", cl::init(false), cl::desc("Trace or Slice"));

static inline std::string
GetFileNameRoot(const std::string &InputFilename) {
  std::string IFN = InputFilename;
  std::string outputFilename;
  int Len = IFN.length();
  if ((Len > 2) &&
      IFN[Len-3] == '.' && IFN[Len-2] == 'b' && IFN[Len-1] == 'c') {
    outputFilename = std::string(IFN.begin(), IFN.end()-3);
  } else {
    outputFilename = IFN;
  }
  return outputFilename;
}

int main(int argc, char **argv) {
  LLVMContext Context;
  llvm_shutdown_obj ShutdownObject;

  try {
    cl::ParseCommandLineOptions(argc, argv, "SAFECode Compiler\n");
    sys::PrintStackTraceOnErrorSignal("");

    std::unique_ptr<Module> M;

    std::unique_ptr<MemoryBuffer> BuffPtr;
    if (auto result = MemoryBuffer::getFileOrSTDIN(InputFilename)) {
      BuffPtr = std::move(*result);
    } else {
      std::cerr << MemoryBuffer::getFileOrSTDIN(InputFilename).getError().message() << "\n";
      return 1;
    }

    if (auto MOrErr = parseBitcodeFile(*BuffPtr, Context)) {
      M = std::move(*MOrErr);
    } else {
      consumeError(MOrErr.takeError());
      M = nullptr;
    }

    if (M.get() == 0) {
      std::cerr << argv[0] << ": bytecode didn't read correctly.\n";
      return 1;
    }

    // Build up the passes (new pass manager). The numbering/Giri analyses are
    // registered on the module analysis manager; the passes below obtain them
    // via MAM.getResult<...>. (Tracer links libgiri/libdgutility directly, so
    // the pipeline is built in code rather than parsed from -passes.)
    ModulePassManager MPM;
    ModuleAnalysisManager MAM;

    // The pipeline is built by hand (no PassBuilder), so the built-in
    // analyses that opt's PassBuilder::registerModuleAnalyses registers for
    // free must be registered here explicitly. 14.0.0's
    // ModulePassManager::run fetches PassInstrumentation from the MAM before
    // running any pass, and VerifierPass::run fetches VerifierAnalysis; with
    // assertions compiled out (the prebuilt 14.0.0 toolchain is Release,
    // assertion-mode OFF) a missing registration is a silent null-deref in
    // AnalysisManager::getResult. Register exactly the analyses this
    // pipeline consumes (no PassBuilder means no other pass can be added).
    static PassInstrumentationCallbacks PIC;
    MAM.registerPass([&] { return PassInstrumentationAnalysis(&PIC); });
    MAM.registerPass([] { return VerifierAnalysis(); });
    MAM.registerPass([] { return dg::QueryBBNumbersPass(); });
    MAM.registerPass([] { return dg::QueryLSNumbersPass(); });
    MAM.registerPass([] { return giri::DynamicGiri(); });

    // The module's DataLayout comes from the parsed bitcode (the frontend
    // emits it); no re-setting is needed. Do NOT write
    // M->setDataLayout(M->getDataLayout()) here: that is a *self-assignment*,
    // and from LLVM 15 DataLayout's defaulted memberwise operator= (SmallVector
    // members share storage) corrupts the parsed alignment table on a
    // self-copy (ABI(i32) 4 -> 65536 in 15.0.0). That corruption then fails
    // the new 15.0.0 verifier check (getABITypeAlign > ParamMaxAlignment=2^14)
    // in the trailing VerifierPass on every pointer-argument call, aborting the
    // tool. (Opt-driven pipelines are unaffected: they never run this line and
    // do not verify by default.)

    // Number all basic blocks and instructions.
    MPM.addPass(dg::BasicBlockNumberPass());
    MPM.addPass(dg::LoadStoreNumberPass());

    if (DoTrace) {
      MPM.addPass(giri::TracingNoGiri());
    } else {
      MPM.addPass(giri::DynamicGiriPass());
    }

    // Remove numbering metadata.
    MPM.addPass(dg::RemoveBasicBlockNumbers());
    MPM.addPass(dg::RemoveLoadStoreNumbers());

    // Verify the final result
    MPM.addPass(VerifierPass());

    // Run the passes.
    MPM.run(*M.get(), MAM);

    // Figure out where we are going to send the output...
    std::string OutputFile;
    if (OutputFilename != "") {
      OutputFile = OutputFilename;
    } else {
      if (InputFilename == "-") {
        OutputFile = "-";
      } else {
        OutputFile = GetFileNameRoot(InputFilename);
        OutputFile += ".sc.bc";
      }
    }

    if (OutputFile != "-") {
      if (!Force && std::ifstream(OutputFile.c_str())) {
        std::cerr << argv[0] << ": error opening '" << OutputFile
                  << "': file exists!\n"
                  << "Use -f command line argument to force output\n";
        return 1;
      }
      std::error_code EC;
      raw_fd_ostream OS(OutputFile, EC, sys::fs::OF_None);
      if (EC) {
        std::cerr << argv[0] << ": error opening " << OutputFile << "!\n";
        return 1;
      }
      // In LLVM 14.0.0 WriteBitcodeToFile is void (it predates the Error
      // return value; hard failures abort internally), so just call it.
      WriteBitcodeToFile(*M, OS);
    } else {
      WriteBitcodeToFile(*M, llvm::outs());
    }

    return 0;
  } catch (const std::string & msg) {
    std::cerr << argv[0] << ": " << msg << "\n";
  } catch (...) {
    std::cerr << argv[0] << ": Unexpected unknown exception occurred.\n";
  }
  llvm_shutdown();
  return 1;
}
