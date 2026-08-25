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
//===----------------------------------------------------------------------===//

#include "Giri/Giri.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Signals.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"

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

    // Build up all of the passes that we want to do to the module...
    legacy::PassManager Passes;
    M->setDataLayout(M->getDataLayout());

    // Number all basic blocks and instructions.
    Passes.add(new BasicBlockNumberPass());
    Passes.add(new LoadStoreNumberPass());

    if (DoTrace) {
      Passes.add(new TracingNoGiri());
    } else {
      Passes.add(new DynamicGiri());
    }

    // Remove numbering metadata.
    Passes.add(new RemoveBasicBlockNumbers());
    Passes.add(new RemoveLoadStoreNumbers());

    // Verify the final result
    Passes.add(createVerifierPass());

    // Run our queue of passes all at once now, efficiently.
    Passes.run(*M.get());

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