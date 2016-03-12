#include "Superblocks.h"
#include <boost/algorithm/string.hpp>
#include "llvm/IR/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/DerivedTypes.h"
#include <fstream>
#include "Common.h"

#define DEBUG_TYPE "pasha_sb"

using namespace llvm;
using namespace sb;
using namespace std;

extern cl::list<std::string> FunctionList;
extern bool isTargetFunction(const Function &f,
                             const cl::list<std::string> &FunctionList);

namespace sb {
    std::function<bool(const Edge&, const Edge&)> getCmp() {
        return [](const Edge& A, const Edge& B) -> bool {
            return A.first < B.first || 
                (A.first == B.first && A.second < B.second);
        };
    }
}

void Superblocks::readSequences() {
    ifstream SeqFile(SeqFilePath.c_str(), ios::in);
    assert(SeqFile.is_open() && "Could not open file");
    string Line;
    for (; getline(SeqFile, Line);) {
        Path P;
        std::vector<std::string> Tokens;
        boost::split(Tokens, Line, boost::is_any_of("\t "));
        P.Id = Tokens[0];

        P.Freq = stoull(Tokens[1]);
        P.PType = static_cast<PathType>(stoi(Tokens[2]));

        move(Tokens.begin() + 4, Tokens.end() - 1, back_inserter(P.Seq));
        Sequences.push_back(P);
        //errs() << *P.Seq.begin() << " " << *P.Seq.rbegin() << "\n";
    }
    SeqFile.close();
}

void
Superblocks::makeEdgeProfile(map<string, BasicBlock*>& BM) {
    for(auto &P : Sequences) {
        auto &Blocks = P.Seq;
        for(unsigned I = 0; I < Blocks.size() - 1 ; I++) {
            auto E = make_pair(BM[Blocks[I]], BM[Blocks[I+1]]);
            if(EdgeProfile.count(E) == 0) {
                EdgeProfile.insert(make_pair(E, APInt(256, 0, false)));
            }
            EdgeProfile[E] += APInt(256, P.Freq, false);
        }
    } 
}

bool 
Superblocks::doInitialization(Module &M) {
    readSequences();
    return false;
}

bool 
Superblocks::doFinalization(Module &M) { return false; }

void 
Superblocks::construct(BasicBlock* Begin, 
        SmallVector<SmallVector<BasicBlock*, 8>, 32>& Superblocks,
        DenseSet<pair<const BasicBlock *, const BasicBlock *>>& BackEdges) {
    BasicBlock* Next = nullptr, *Prev = nullptr;
    SmallVector<BasicBlock*, 8> SBlock;
    SBlock.push_back(Begin);
    Prev = Begin;
    do {
        if(Next != nullptr) {
            SBlock.push_back(Next);
            Prev = Next;
        }
        Next = nullptr;
        APInt Count(256, 0, false);
        for(auto SB = succ_begin(Prev), SE = succ_end(Prev);
                SB != SE; SB++) { 
            auto E = make_pair(Prev, *SB);
            if(!BackEdges.count(E) && EdgeProfile.count(E) != 0 ) {
                if(EdgeProfile[E].ugt(Count)) {
                    Count = EdgeProfile[E];
                    Next = *SB;
                } 
            }
        }
    } while (Next);

    Superblocks.push_back(SBlock);
}

void 
Superblocks::process(Function &F) {
    map<string, BasicBlock *> BlockMap;
    for (auto &BB : F)
        BlockMap[BB.getName().str()] = &BB;

    makeEdgeProfile(BlockMap);
    auto BackEdges = common::getBackEdges(&F.getEntryBlock());
    
    SmallVector<SmallVector<BasicBlock*, 8>, 32>  Superblocks;
    LoopInfo &LI = getAnalysis<LoopInfo>(F);
    vector<Loop*> InnerLoops = getInnermostLoops(LI);
    errs() << "Num Loops: " << InnerLoops.size() << "\n";
    for(auto &L : InnerLoops) {
        assert(L->getHeader()->getParent() == &F);
        construct(L->getHeader(), Superblocks, BackEdges);
    }

    for(auto &SV : Superblocks) {
        for(auto &BB : SV) {
            errs() << BB->getName() << " ";
        }
        errs() << "\n\n";
    }
}

bool 
Superblocks::runOnModule(Module &M) {
    for (auto &F : M)
        if (isTargetFunction(F, FunctionList))
            process(F);

    return false;
}

char Superblocks::ID = 0;