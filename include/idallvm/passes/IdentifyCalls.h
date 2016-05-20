#ifndef _IDALLVM_PASSES_IDENTIFYCALLS_H
#define _IDALLVM_PASSES_IDENTIFYCALLS_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct IdentifyCalls : public llvm::FunctionPass
{
    static char ID;
    IdentifyCalls();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
    bool runOnFunction_Arm(llvm::Function& f);
};

#endif /* _IDALLVM_PASSES_IDENTIFYCALLS_H */
