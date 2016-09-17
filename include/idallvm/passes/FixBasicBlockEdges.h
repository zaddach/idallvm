#ifndef _IDALLVM_PASSES_FIXBASICBLOCKEDGES_H
#define _IDALLVM_PASSES_FIXBASICBLOCKEDGES_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct FixBasicBlockEdges : public llvm::FunctionPass
{
    static char ID;
    FixBasicBlockEdges();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_FIXBASICBLOCKEDGES_H */
