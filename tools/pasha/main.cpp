#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"

#include <string>
#include <thread>
#include <fstream>

#include "GraphGrok.h"

using namespace std;
using namespace llvm;
using namespace llvm::sys;
using namespace grok;

namespace {
cl::opt<string> InPath(cl::Positional, cl::desc("<Module to analyze>"),
                       cl::value_desc("bitcode filename"), cl::Required);

cl::opt<string> SeqFilePath("seq",
                            cl::desc("File containing basic block sequences"),
                            cl::value_desc("filename"),
                            cl::init("epp-sequences.txt"));

cl::opt<int> NumSeq("num", cl::desc("Number of sequences to analyse"),
                    cl::value_desc("positive integer"), cl::init(3));
}

int main(int argc, char **argv, const char **env) {
    // This boilerplate provides convenient stack traces and clean LLVM exit
    // handling. It also initializes the built in support for convenient
    // command line option handling.

    sys::PrintStackTraceOnErrorSignal();
    llvm::PrettyStackTraceProgram X(argc, argv);
    LLVMContext &context = getGlobalContext();
    llvm_shutdown_obj shutdown;

    // InitializeAllTargets();
    // InitializeAllTargetMCs();
    // InitializeAllAsmPrinters();
    // InitializeAllAsmParsers();
    // cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);
    cl::ParseCommandLineOptions(argc, argv);

    // Construct an IR file from the filename passed on the command line.
    SMDiagnostic err;
    unique_ptr<Module> module(parseIRFile(InPath.getValue(), err, context));

    if (!module.get()) {
        errs() << "Error reading bitcode file.\n";
        err.print(argv[0], errs());
        return -1;
    }

    PassManager PM;
    PM.add(createBasicAliasAnalysisPass());
    PM.add(createTypeBasedAliasAnalysisPass());
    PM.add(llvm::createPostDomTree());
    PM.add(new DominatorTreeWrapperPass());
    PM.add(new grok::GraphGrok(SeqFilePath, NumSeq));
    PM.run(*module);

    // for(auto &V : Sequences)
    // for(auto &E : V)
    // errs() << E << "\n";

    return 0;
}
