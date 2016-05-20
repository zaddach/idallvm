#ifndef _IDALLVM_PASSES_INLINEOPCODECALLS_H
#define _IDALLVM_PASSES_INLINEOPCODECALLS_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct InlineOpcodeCalls : public llvm::FunctionPass
{
    static char ID;
    InlineOpcodeCalls();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_INLINEOPCODECALLS_H */
