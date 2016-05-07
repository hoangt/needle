#ifndef EPPENCODE_H
#define EPPENCODE_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/MapVector.h"

#include <map>
#include <unordered_map>
#include <queue>


namespace epp {


enum EdgeType { EHEAD,
                ELATCH,
                ELIN,
                ELOUT1, // To Exit
                ELOUT2, // From Entry 
                EREAL,
                EOUT };


struct Edge {
    llvm::BasicBlock *Src, *Tgt;
    EdgeType Type;
    uint64_t Id;
    Edge(llvm::BasicBlock *S, llvm::BasicBlock *T, EdgeType ET)
        : Src(S), Tgt(T), Type(ET) {}
    llvm::BasicBlock *src() { return Src; }
    llvm::BasicBlock *tgt() { return Tgt; }
    static std::shared_ptr<Edge> makeEdge(llvm::BasicBlock *S,
                                          llvm::BasicBlock *T, EdgeType ET) {
        return std::make_shared<Edge>(S, T, ET);
    }
    bool operator==(const Edge& E) {
        if(E.Src == Src &&
           E.Tgt == Tgt &&
           E.Type == Type)
            return true;
        return false;
    }
};

struct EPPEncode : public llvm::FunctionPass {

    static char ID;

    llvm::LoopInfo *LI;
    uint64_t selfLoopCounter;
    llvm::DenseMap<llvm::BasicBlock *, llvm::APInt> numPaths;
    llvm::DenseMap<uint64_t, llvm::BasicBlock *> selfLoopMap;
    std::unordered_map<std::shared_ptr<Edge>, llvm::APInt> Val;
    std::unordered_map<std::shared_ptr<Edge>, llvm::APInt> Inc;

    EPPEncode() : llvm::FunctionPass(ID), LI(nullptr), selfLoopCounter(0) {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage &au) const override {
        au.addRequired<llvm::LoopInfo>();
        au.setPreservesAll();
    }

    virtual bool runOnFunction(llvm::Function &f) override;
    void encode(llvm::Function &f);
    bool isTargetFunction(llvm::Function &f,
                          llvm::cl::list<std::string> &FunctionList) const;
    bool doInitialization(llvm::Module &m) override;
    bool doFinalization(llvm::Module &m) override;
    virtual void releaseMemory() override;
    const char* getPassName() const override {
        return "PASHA - EPPEncode";
    }
};
}

#endif
